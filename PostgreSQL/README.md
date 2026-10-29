# HMS-T-02: PostgreSQL schema and migrations

This is a standalone database component based on the supplied HMS Architecture and Design Specification (SAD), especially sections 3.6, 3.7 and 4.1, plus the T-02 backlog description. It is intended to be integrated into the main repository after T-01 establishes the project structure.

## Included

- PostgreSQL 15 via Docker Compose.
- Ordered SQL initialization files in `database/migrations/`.
- Keyed blind-index columns and indexes for patient name-prefix and phone search.
- AES-256-GCM encryption and database-backed patient registration, update, and search.
- Database-backed user authentication and hashed bearer sessions.
- Tables for users, patients, patient history, doctors, appointments, visits, prescriptions, bills, bill items, payments and audit log.
- Foreign keys, check constraints, uniqueness constraints and indexes.
- Partial unique index preventing two `Confirmed` appointments for the same doctor and slot.
- Insert-only enforcement for `audit_log` rows.
- Development-only seed users for all five roles, a doctor and two sample patients.
- SQL verification script.

## Start the database

From this directory, with Docker Desktop/Docker Engine running:

```sh
docker compose up -d
```

Wait until it is healthy:

```sh
docker compose ps
```

Connection settings for local development:

- Host: `localhost`
- Port: `5432`
- Database: `hms`
- User: `hms_app`
- Password: `local_dev_only_change_me` by default (set `POSTGRES_PASSWORD` in `.env` to override)

Run the verification checks:

```sh
docker compose exec -T db psql -U hms_app -d hms < database/tests/verify.sql
```

The API server uses libpq environment variables (`PGHOST`, `PGPORT`,
`PGDATABASE`, `PGUSER`, and `PGPASSWORD`) to connect to this database. On
Windows, ensure PostgreSQL's `bin` directory is on `PATH` so the server can
load `libpq.dll`. Do not put production database passwords in source control.

Patient management is available to authenticated Receptionist users:

- `POST /api/v1/patients` registers demographic fields `name`, `dateOfBirth`
  (`DD-MM-YYYY`), `gender`, `phone`, and `address`.
- If a patient with the same normalized name, date of birth, and phone number
  already exists, registration returns `409 DUPLICATE_PATIENT` without saving.
  After the user confirms the warning, retry the same request with
  `?confirmDuplicate=true` to save the record.
- `PUT /api/v1/patients/{patientId}` updates those fields and writes previous
  values to encrypted `patient_history`.
- `GET /api/v1/patients/search?query=...` searches by patient ID, exact phone,
  or case-insensitive name prefix. Receptionists and Doctors may search.
- `GET /api/v1/patients/{patientId}/history` retrieves prior demographic
  values newest-first. Receptionists and Doctors may view the history. Each
  entry includes `changedAt` (UTC ISO-8601), `changedBy` (username), and
  `oldValues` (`name`, `dateOfBirth`, `gender`, `phone`, and `address`):

  ```json
  {
    "history": [{
      "changedAt": "2026-10-21T12:00:00.000000Z",
      "changedBy": "receptionist",
      "oldValues": {
        "name": "Asha Nair",
        "dateOfBirth": "04-05-1980",
        "gender": "Female",
        "phone": "9876543210",
        "address": "12 Example Road"
      }
    }]
  }
  ```

Set `HMS_PATIENT_DATA_KEY` to a secret containing exactly 32 bytes and set
`HMS_PATIENT_NAME_INDEX_KEY` and `HMS_PATIENT_PHONE_INDEX_KEY` to different
secrets of at least 32 bytes each; all three keys must be distinct. Values are raw
bytes, not hexadecimal encodings; use a secret manager or protected process
environment, never source control. AES-256-GCM encrypts patient name, birth
date, phone, address, and history values with independent nonces and
field-specific authenticated data. Existing demo ciphertext is not encrypted
with these keys and its blind indexes cannot be searched; backfill real records
with the application encryption and index keys before relying on search.

The server accepts patient self-registration at `POST /api/v1/auth/register`
and login at `POST /api/v1/auth/login`. It returns a random bearer token on
successful login, persists only the token's SHA-256 hash, expires sessions
after 10 minutes of inactivity, and revokes them at `POST /api/v1/auth/logout`.
Protected endpoints require `Authorization: Bearer <token>`. Configure the first
administrator only when starting the server by setting both
`HMS_BOOTSTRAP_ADMIN_USERNAME` and `HMS_BOOTSTRAP_ADMIN_PASSWORD`; the
credentials are never given defaults. An authenticated administrator can
provision role accounts through `POST /api/v1/users`. Public self-registration
always creates a Patient account.

Administrators can deactivate an account with
`PATCH /api/v1/users/{userId}` and the JSON body
`{"action":"deactivate"}`. A successful request returns HTTP 200 with
`{"userId":"…","active":false,"message":"User deactivated."}`.
The endpoint requires a bearer session with `ManageUsers` permission. It
returns 400 for malformed IDs or bodies, 401 when authentication is missing
or invalid, 403 for insufficient permission, 404 for an unknown user, 409
when the request would deactivate the acting administrator or the last active
Administrator, 415 when the body is not JSON, and 500 when the operation
cannot be completed. Errors use the normal API envelope:
`{"error":{"code":"…","message":"…","requestId":"…"}}`.
Account status and its `DEACTIVATE_USER` audit event are committed in one
database transaction. Existing sessions for an inactive account stop
authenticating immediately.

The API writes successful logins, failed login attempts, logout, account and
patient views, patient/appointment/visit/prescription creation, and patient
updates to the insert-only `audit_log` table. Each event stores the user ID
(when known), role, action, affected record ID (when applicable), database
timestamp, and peer IP address from the TLS connection. Failed login events
record the attempted username only when it is within the normal username
length limit. Audit writes use parameterized SQL; passwords, bearer tokens,
and patient field contents are not included in audit records.

New passwords must be at least eight characters and include uppercase,
lowercase, digit, and special characters. New password hashes use Argon2id with
a random salt (three iterations, 64 MiB memory, one lane). Existing
PBKDF2-HMAC-SHA-256 hashes are verified and upgraded to Argon2id after a
successful login, retaining accounts created by older application builds.

The build requires the Argon2 development library. On Linux install
`libargon2-dev`; on Windows with MSYS2/UCRT64 install
`mingw-w64-ucrt-x86_64-argon2`. For a nonstandard install, pass its header and
library paths through `ARGON2_INCLUDE_DIR` and `ARGON2_LIBRARY`.

Doctors can create a consultation for one of their confirmed appointments with
`POST /api/v1/visits` and JSON string fields `appointmentId`, `symptoms`,
`diagnosis`, and `notes`. The API returns the visit ID. Add a prescription with
`POST /api/v1/visits/{visitId}/prescriptions` and JSON string fields `item`,
`dosage`, `frequency`, and `days`. Visit text is encrypted with AES-256-GCM
before it is stored in the existing `BYTEA` columns, using the same 32-byte
`HMS_PATIENT_DATA_KEY`; prescriptions can only be added by the doctor who
recorded the visit.

An administrator-created Doctor account is provisioned with a default
weekday 09:00-17:00 schedule. The user-creation response includes its doctor
ID. A Patient account can create its own linked patient profile from the
desktop client; the generated patient ID is used when booking.

Appointments can be booked with `POST /api/v1/appointments` using string fields
`patientId`, `doctorId`, `date` (`YYYY-MM-DD`), and `time` (`HH:MM`). Retrieve
available slots with `GET /api/v1/appointments/free-slots?doctorId=...&date=...`.
Patient accounts may book only for a patient record linked to their user ID.
Conflicts return HTTP 409 with up to three alternative slots in
`suggestedSlots`.

Billing Officers and Administrators can download daily appointment and revenue
reports with
`GET /api/v1/reports?startDate=YYYY-MM-DD&endDate=YYYY-MM-DD&format=csv`
or `format=pdf`. The date range is inclusive. Each day includes appointment
counts by status and payments received on that day. Successful responses are
downloadable attachments; malformed dates, reversed ranges, and unsupported
formats return HTTP 400.

## Run the API and desktop client over TLS

The API listens only on `127.0.0.1:8443` and requires HTTPS with TLS 1.2 or
higher. It has no plaintext HTTP listener. Build the Windows server with
OpenSSL and Argon2 development files available to CMake (`OPENSSL_ROOT_DIR`,
`ARGON2_INCLUDE_DIR`, and `ARGON2_LIBRARY` may be set when those libraries are
outside their default search paths) and keep the matching runtime DLLs on the
server's DLL search path. Configure these
environment variables before starting `hms_server`:

- `HMS_TLS_CERT_FILE`: PEM certificate chain with a `localhost` DNS name and
  `127.0.0.1` IP subject alternative name.
- `HMS_TLS_KEY_FILE`: matching PEM private key. Keep this file private and out
  of source control.

The desktop client defaults to `https://127.0.0.1:8443`. Set
`HMS_API_BASE_URL` to another HTTPS server origin when needed; the client
refuses plain HTTP and validates the server certificate using the platform's
trusted certificate authorities. For local development, use a certificate
issued by a CA trusted by the client machine. Do not disable certificate
validation or commit development certificates/private keys. The Qt deployment
must also include a working Qt TLS backend.

## Health checks and availability target (HMS-NF-004)

The TLS API exposes two unauthenticated GET probes:

- `GET /api/v1/health/live` returns HTTP 200 while the process can serve the
  request. It checks liveness only and does not depend on PostgreSQL.
- `GET /api/v1/health` checks readiness with a lightweight `SELECT 1` using a
  short-lived PostgreSQL connection. It returns HTTP 200 when ready and HTTP
  503 when PostgreSQL is unavailable. The query uses a one-second PostgreSQL
  `statement_timeout`, and connection attempts use a two-second
  `connect_timeout`. A fresh connection per probe lets readiness recover after
  a database outage without disturbing the authentication connection.
  Responses report component states and do not include credentials or
  connection data.

These endpoints do not by themselves guarantee or measure 99.5% availability.
An independent monitor must poll readiness and retain each probe's UTC
timestamp and result. Missing measurements count as downtime. Use one-minute
probe intervals or shorter; scheduled maintenance is included as downtime.

For each report, choose a fixed measurement period (the recommended report is
the most recent 30 complete service days) and pass an explicit UTC offset in
minutes to `hms::health::calculate_availability`. The calculator applies the
local service window `[06:00, 22:00)`, counts unmeasured intervals as downtime,
and reports eligible time, available time, downtime, percentage, and whether
the 99.5% target was met. It never reads the machine's local timezone. For a
timezone that changes offset seasonally, split reports at each offset change
and calculate each segment with its applicable offset before combining
durations. For a full 16-hour service day, 99.5% permits at most 4.8 minutes of
downtime; over 30 complete days the eligible period is 480 hours and the
corresponding allowance is 144 minutes.

The calculator does not store monitoring records. Operators must retain probe
data and alert on missing probes and failed readiness checks. Loss of monitor
records makes the affected period unverified and it must not be reported as
available. Deployment monitoring and durable retention are still required to
demonstrate compliance.

The client clears its local bearer token on Logout and after 10 minutes without
keyboard or pointer activity, then requests server-side token revocation. If
the server cannot be reached during logout, the client warns that remote
revocation could not be confirmed; server-side inactivity expiry still limits
the token lifetime.

## Appointment booking concurrency target (HMS-NF-002)

The API accepts requests on a bounded pool of 12 workers. Each worker owns
separate authentication, patient, appointment, and clinical PostgreSQL
repositories; a repository/connection is never shared between request threads.
The listener also performs TLS handshakes on workers, while the pending socket
queue is capped at 100 connections. Each server instance therefore holds up
to 60 application database connections, in addition to PostgreSQL's reserved
and other application connections. Size the database connection limit
accordingly.

The appointment load test sends 100 booking requests concurrently and measures
the end-to-end HTTP response latency. Prepare a JSON scenario file with exactly
100 distinct, valid `patientId`, `doctorId`, `date`, `time`, and `token` string
fields per request. Each token must belong to a user permitted to book the
associated patient, and each appointment slot must be free. Keep tokens out of
source control and remove the scenario file securely after testing. Successful
requests create real appointment and audit records; use a test database and
fresh slots for each run. To match the NF-002 workload, populate the test
database with 100,000 patient records and provision 100 booking users before
the run; each user submits one booking in the simultaneous batch.

Run the server and test from the same host, with PostgreSQL and the required
TLS and patient-key environment variables configured:

```sh
python scripts/appointment_booking_load_test.py \
  --base-url https://localhost:8443 \
  --scenario appointment-bookings.json \
  --ca-file path/to/test-ca.pem
```

Omit `--ca-file` only when the server certificate chains to a CA trusted by
the host. The script reports successful bookings, failures, p50, p95, and
maximum latency, and exits nonzero unless every request succeeds and p95 is
under 3 seconds. Record the measured output with the server and database
configuration; code changes alone do not demonstrate that the 95th-percentile
target is met. Run the command from the project repository root.

Stop the database:

```sh
docker compose down
```

To delete the local database volume and start from a clean database (this permanently deletes local DB data):

```sh
docker compose down -v
docker compose up -d
```

## Important notes before integration

1. **Docker init scripts run only on an empty data volume.** If you edit a migration after first startup, reset the local volume or apply the new migration manually. For a shared/team environment, use a migration ledger/runner so already-applied migrations are tracked. The SQL files are versioned, but this initial standalone component does not yet include a C++/libpqxx migration runner because T-01's application scaffold is not in the supplied ZIP.
2. **Patient encryption and updates are implemented in the C++ data layer.** PostgreSQL stores ciphertext in `BYTEA`; demo values are labelled placeholders, not ciphertext. Encryption keys must be kept outside PostgreSQL and source control.
3. **Patient search uses keyed blind indexes.** Migration `004_patient_search_blind_indexes.sql` adds a GIN-indexed array of HMAC-SHA-256 name prefixes and a B-tree-indexed HMAC of the phone number. Name search is prefix-based and case-insensitive for ASCII; it does not match a substring in the middle of a name. The application uses distinct keys supplied through protected environment configuration. Blind indexes reveal equality patterns and are vulnerable to dictionary attacks if their keys are compromised. Rotate keys and rebuild indexes through a controlled migration procedure. Existing patient rows are not backfilled by SQL because PostgreSQL does not have the HMAC keys.
4. **Patient ID sequence migration.** Migration `006_patient_id_sequence.sql` creates the sequence used for concurrent-safe IDs. Docker initialization scripts run only on an empty data volume; apply migrations `004` through `006` manually to existing databases before running the updated server.
5. **Seed login credentials are intentionally disabled.** Seed `password_hash` values are sentinel strings, not valid Argon2id hashes. Replace them with hashes generated by the application before attempting login; no real passwords are included in this repo.
6. The SAD's short table overview does not specify every column or every enum value. The added columns/constraints here are implementation choices to support referential integrity and common workflows; review them with the team before merge.
7. PostgreSQL cannot make a table absolutely immutable against its owner/superuser. The trigger prevents ordinary row-level UPDATE/DELETE statements; production permissions should additionally grant the application role INSERT/SELECT only on `audit_log`.
8. The SQL migrations and verification script have been run successfully against a fresh local PostgreSQL 18 database (`hms_t02_test`). Docker Compose has not yet been tested; validate it separately if Docker is available before claiming the Compose setup was tested.
