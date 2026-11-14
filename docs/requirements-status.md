# Requirements implementation and verification status

This review compares the SRS, Architecture and Design Specification (SAD), and
Test Plan against the code and available test/CI evidence. “Implemented” means
the behavior exists in the source and has related automated tests; it does not
mean a production or user-acceptance test has been completed.

## Functional requirements

| Requirement | Status and evidence |
| --- | --- |
| HMS-F-001–004 Authentication, lockout, RBAC, generic login errors | Implemented in `src/services/auth_service.*`, `src/services/rbac_service.*`, and `src/main.cpp`; covered by `tests/auth_service_test.cpp` and `tests/api_test.cpp`. |
| HMS-F-010–014 Patient registration, validation, duplicate warning, search, update history | Implemented in `src/patients/`, API routes in `src/main.cpp`, and client screens in `src/client/patientmanagementwindow.cpp`; covered by patient service and encryption tests. The client now includes the optional allergy field supported by the API. |
| HMS-F-020–024 Booking, unique-slot conflict, booking window, reschedule/cancel, doctor schedule | Implemented in `src/appointments/` and `src/main.cpp`; covered by appointment, change-appointment, and doctor-schedule tests. See the documented F-024 response deviations below. |
| HMS-F-030–034 Visits, prescriptions, allergy warning/override, history, patient-owned records | Service and API behavior exists in `src/clinical/`, `src/billing/`, and `src/main.cpp`; covered by clinical service tests. The desktop client records visits and prescriptions, prompts for a required allergy-override reason, loads doctor appointments with patient identity/allergy context, and exposes patient-owned history and bills. |
| HMS-F-040–042 Itemized bills, payments, receipts | Implemented in `src/billing/` and API routes in `src/main.cpp`; covered by billing, tax-boundary, and receipt tests. The Billing Officer desktop workflow supports itemized bill creation, payment recording, and receipt downloads. |
| HMS-F-050 Daily appointment/revenue reports | CSV/PDF report generation and API route are implemented in `src/reports/` and `src/main.cpp`; covered by `tests/report_service_test.cpp`. The desktop client exposes date-filtered report generation and file saving. |
| HMS-F-060 User creation, deactivation, role assignment | Server-side creation/deactivation and role checks are implemented. The Administrator desktop workflow lists accounts, creates and deactivates accounts, and changes supported roles. Role changes to/from Doctor are rejected until doctor-profile management can be performed atomically; the last active Administrator cannot be deactivated or demoted. |
| HMS-F-061 Filtered administrator audit log | Implemented in `src/api/audit_log_handler.cpp` with tests under the auth/API suites. The Administrator desktop workflow exposes filtered audit viewing, and the API returns the SAD-specified array of entries. |

## Non-functional and security requirements

| Requirement | Status and evidence |
| --- | --- |
| HMS-NF-001 Patient search: p95 ≤ 2 seconds at 100k records / 100 users | Load-test assets and a seed utility exist under `tests/performance/`; no completed NF-001 benchmark result is checked in. |
| HMS-NF-002 Booking: p95 < 3 seconds under the specified load | `scripts/appointment_booking_load_test.py` implements the 100-request workload and threshold. No current result artifact demonstrates the target. |
| HMS-NF-003 200 concurrent sessions for 30 minutes, error rate < 1% | `tests/performance/NF-003-summary.txt` records a 30-minute, 200-session run with 0.411% errors and a surviving server (reported PASS). This is a checked-in test result, not a guarantee of other deployments. |
| HMS-NF-004 99.5% service-window availability | Health/readiness endpoints and an availability calculator exist. Independent polling, durable monitoring records, and a calendar-month availability report are operational requirements and have not been established by the code alone. |
| HMS-NF-005 Patient registration in ≤ 3 minutes | The receptionist client times registration and validates the required fields. The manual timing acceptance check in `docs/usability/NF-005.md` has no recorded outcome. |
| HMS-NF-006 Encrypted 24-hour backup, restore ≤ 4 hours, RPO ≤ 24 hours | Encrypted backup/restore code and tests exist. Windows scheduling is a separately registered operational task; no production-scale four-hour recovery drill is evidenced. |
| HMS-NF-007 Windows 10/11 and Ubuntu 22.04, GCC 11+/MSVC 2019+ | CI builds Ubuntu 22.04 and Windows Server 2022 with MSVC 2022. The documented MSVC 2019 and interactive Windows 10/11 desktop checks remain unverified. |
| HMS-NF-008 At least 70% line coverage per service | The GCC/lcov CI job enforces the threshold using the explicit service inventory in `CMakeLists.txt` and `scripts/check_service_coverage.py`. It is not a claim about excluded API/UI/integration code. |
| HMS-NF-009 At least 90% independent completion over five users | `docs/usability/NF-009.md` defines the 50-attempt protocol. No five-participant results are recorded; do not mark this requirement passed yet. |
| HMS-SEC-001 Password hashing and strength | Argon2id hashing and password validation exist in auth/password-security services; related automated tests are present. |
| HMS-SEC-002 TLS 1.2+, no HTTP | The server configures TLS 1.2 or newer and listens for TLS; the client requires HTTPS and platform certificate validation. |
| HMS-SEC-003 Ten-minute inactivity expiry and logout revocation | Server-side session checks and client idle/logout behavior are implemented and covered by auth tests. |
| HMS-SEC-004 Input validation and parameterized database access | Validation and parameterized PostgreSQL access are used in service repositories and API routes; service tests cover key validation paths. |
| HMS-SEC-005 Audit records for access/change events | API routes write audit events for supported reads and mutations. The audit data does not include credentials or clinical payloads. |
| HMS-SEC-006 AES-256 encryption at rest | Patient demographics/history and clinical text are encrypted; field-encryption and clinical service tests cover the cryptographic behavior. |
| HMS-SEC-007 Server-side authorization | API permission and ownership checks are implemented in the server and auth tests. UI visibility is not the authorization boundary. |
| HMS-SEC-008 Restricted, content-safe audit log | Audit reads require the administrator permission; audit storage is insert-only and excludes passwords/tokens/clinical text. API/service tests and the SQL verification script cover parts of this behavior. |

## Known contract deviations and integration gaps

- Remaining F-024 and F-042 contract differences are recorded in
  [deviations.md](deviations.md) and should be resolved against the SAD before
  external clients depend on the exact response or endpoint shapes.
- Role transitions to or from Doctor remain intentionally unavailable in the
  account-management workflow because they require synchronized doctor-profile
  management. Creating a Doctor account remains supported.
- Newly added `/api/v1/users` listing and role-update routes are not yet covered
  by running-server end-to-end tests. The existing API test suite primarily
  exercises API helpers and service-level handlers.
- Automated CTest does not replace the manual usability, operational
  availability, production-scale load, or recovery-drill acceptance criteria.
- `tests/api_test.cpp` tests API error/JSON helpers rather than exercising the
  running TLS routes end to end. Add route-level integration tests before
  treating service-unit coverage as proof of the complete API contract.

## Validation snapshot

The prior 2026-11-12 baseline recorded a Windows Visual Studio 2022 x64
Release build, all 16 registered CTest suites, three coverage-checker unit
tests, and an offscreen Qt startup smoke check. Its PostgreSQL-backed GCC
13.3/lcov run measured 92.09% service-source line coverage (2,805/3,046) and
51.62% branch coverage (3,618/7,009). Those coverage figures predate the
desktop integration changes in this branch and should not be treated as
current measurements. Detailed baseline evidence and limitations are in
[docs/reports/](reports/) and the updated test cases in
[docs/test-cases-updated.md](test-cases-updated.md).

For the follow-up changes, the existing WSL/GCC build completed, all 16
registered CTest suites passed, and the Qt client stayed running through a
bounded four-second offscreen startup check. The PostgreSQL-specific account
tests were not rerun because their dedicated `hms_f060_test` database is not
present. A fresh Windows Visual Studio build and post-change coverage capture
remain outstanding.

The latest available historical GitHub Actions service coverage run is
2026-10-31 at commit `82e117d935ecc80572398c215f187adad0e206a8`; its line
coverage passed the configured per-module threshold. The measured 2026-11-12
working-tree result is local evidence and is not yet a completed GitHub Actions
run. Live API and PostgreSQL system validation, manual acceptance tests, and
operational availability/recovery tests remain incomplete.
