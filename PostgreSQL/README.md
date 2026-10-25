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
- `PUT /api/v1/patients/{patientId}` updates those fields and writes previous
  values to encrypted `patient_history`.
- `GET /api/v1/patients/search?query=...` searches by patient ID, exact phone,
  or case-insensitive name prefix. Receptionists and Doctors may search.

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

The client clears its local bearer token on Logout and after 10 minutes without
keyboard or pointer activity, then requests server-side token revocation. If
the server cannot be reached during logout, the client warns that remote
revocation could not be confirmed; server-side inactivity expiry still limits
the token lifetime.

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
