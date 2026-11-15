# Requirements implementation and verification status

This review compares the SRS, Architecture and Design Specification (SAD), and
Test Plan against the code and available test/CI evidence. “Implemented” means
the behavior exists in the source and has related automated tests; it does not
mean a production or user-acceptance test has been completed.

## Sprint submission summary

The sprint test-document deliverables are prepared: the test cases were updated
for sprint changes, the updated test-case document is present, and the four
requested reports (Unit Test, System Validation, Code Coverage, and Branch
Coverage) are in `docs/reports/`. Automated evidence includes 16 CTest suites
passing on Ubuntu and Windows CI, a PostgreSQL-enabled coverage run, and a
four-second Qt startup smoke check. Service line coverage measured 91.66% and
branch coverage measured 54.69%; the Test Plan sets no branch-coverage
threshold. On 2026-11-15, local Windows and WSL reruns again passed all 16
registered CTest suites. Read-only checks against the running local server
also passed TLS health/readiness and confirmed unauthenticated user listing is
rejected.

The project owner confirms that 34 of the 40 Test Plan cases were completed
through manual and automated testing. Six resource-intensive acceptance cases
are deferred from this sprint submission: TC-PERF-01, TC-PERF-02, TC-PERF-03
current rerun, TC-AVL-01, TC-USA-01, and TC-REL-01. The 34-case completion
status is based on the owner's report; see the System Validation Report for
automated evidence and the deferred-case resource requirements.

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
| HMS-NF-001 Patient search: p95 ≤ 2 seconds at 100k records / 100 users | Load-test assets and a seed utility exist under `tests/performance/`; acceptance execution is deferred as TC-PERF-01 because it requires a 100,000-patient dataset and 100 concurrent users. |
| HMS-NF-002 Booking: p95 < 3 seconds under the specified load | `scripts/appointment_booking_load_test.py` implements the workload and threshold; acceptance execution is deferred as TC-PERF-02 because it requires a dedicated appointment load environment and test data. |
| HMS-NF-003 200 concurrent sessions for 30 minutes, error rate < 1% | `tests/performance/NF-003-summary.txt` records a 30-minute, 200-session run with 0.411% errors and a surviving server (reported PASS). A current rerun is deferred as TC-PERF-03. |
| HMS-NF-004 99.5% service-window availability | Health/readiness endpoints and an availability calculator are implemented; the seven-day measurement is deferred as TC-AVL-01. |
| HMS-NF-005 Patient registration in ≤ 3 minutes | The receptionist client times registration and validates the required fields. The five-user timing/usability protocol is deferred as TC-USA-01 until trained participants are available. |
| HMS-NF-006 Encrypted 24-hour backup, restore ≤ 4 hours, RPO ≤ 24 hours | Encrypted backup/restore code and automated tests are implemented; the timed operational recovery drill is deferred as TC-REL-01. |
| HMS-NF-007 Windows 10/11 and Ubuntu 22.04, GCC 11+/MSVC 2019+ | The project owner reports portability checks completed. Repository CI evidence records Ubuntu 22.04 and Windows Server 2022 with MSVC 2022; detailed results for each required OS/compiler combination were not included in the test record. |
| HMS-NF-008 At least 70% line coverage per service | The GCC/lcov CI job enforces the threshold using the explicit service inventory in `CMakeLists.txt` and `scripts/check_service_coverage.py`. It is not a claim about excluded API/UI/integration code. |
| HMS-NF-009 At least 90% independent completion over five users | `docs/usability/NF-009.md` defines the 50-attempt protocol. The five-user acceptance measurement is deferred as TC-USA-01 until the required trained participants are available. |
| HMS-SEC-001 Password hashing and strength | Argon2id hashing and password validation exist in auth/password-security services; related automated tests are present. |
| HMS-SEC-002 TLS 1.2+, no HTTP | The server configures TLS 1.2 or newer and listens for TLS; the client requires HTTPS and platform certificate validation. |
| HMS-SEC-003 Ten-minute inactivity expiry and logout revocation | Server-side session checks and client idle/logout behavior are implemented and covered by auth tests. |
| HMS-SEC-004 Input validation and parameterized database access | Validation and parameterized PostgreSQL access are used in service repositories and API routes; service tests cover key validation paths. |
| HMS-SEC-005 Audit records for access/change events | API routes write audit events for supported reads and mutations. The audit data does not include credentials or clinical payloads. |
| HMS-SEC-006 AES-256 encryption at rest | Patient demographics/history and clinical text are encrypted; field-encryption and clinical service tests cover the cryptographic behavior. |
| HMS-SEC-007 Server-side authorization | API permission and ownership checks are implemented in the server and auth tests. UI visibility is not the authorization boundary. |
| HMS-SEC-008 Restricted, content-safe audit log | Audit reads require the administrator permission; audit storage is insert-only and excludes passwords/tokens/clinical text. API/service tests and the SQL verification script cover parts of this behavior. |

## Scope notes and follow-up evidence

- The F-024 schedule example differences and the supplemental F-042 receipt
  retrieval route are recorded in [deviations.md](deviations.md).
- Role transitions to or from Doctor remain intentionally unavailable in the
  account-management workflow because they require synchronized doctor-profile
  management. Creating a Doctor account remains supported.
- Newly added `/api/v1/users` listing and role-update routes are not yet covered
  by running-server end-to-end tests. The existing API test suite primarily
  exercises API helpers and service-level handlers.
- Six resource-intensive acceptance cases are deferred for this submission;
  their IDs and required resources are listed in the [updated test-case
  document](test-cases-updated.md).
- `tests/api_test.cpp` tests API error/JSON helpers rather than exercising the
  running TLS routes end to end. Add route-level integration tests before
  treating service-unit coverage as proof of the complete API contract.

## Validation snapshot

The latest application CI run completed Windows and Ubuntu builds and all 16
registered CTest suites on both platforms. A separate PostgreSQL-enabled
coverage job also passed all 16 suites, including the configured repository
and backup integration paths. The Qt desktop client passed a bounded
four-second offscreen startup smoke check.

The refreshed GCC/gcov/lcov reports are based on the latest successful
branch-enabled coverage artifact, captured from commit `b170901` in GitHub
Actions run 43625440994. Service-source line coverage is 2,848/3,107 (91.66%);
every service module exceeds the 70% line threshold. Branch coverage is
3,582/6,550 (54.69%). No minimum branch-coverage threshold is specified in
the Test Plan. Per-module data and limitations are in the dated reports under
[docs/reports/](reports/), and the test execution record is in
[docs/test-cases-updated.md](test-cases-updated.md).

The detailed System Validation Report records the 34 cases completed per
project-owner confirmation, the automated checks, and the six deferred
resource-intensive cases. Daksha's assigned-case execution records are in
[Test-Execution-Log-Daksha.md](reports/Test-Execution-Log-Daksha.md); they
record 17 locally executed automated component-evidence cases and three
deferred performance cases, without claiming unsupported manual acceptance
passes. The optional dedicated `hms_f060_test` PostgreSQL database was not
available for a separate local rerun of the account-management integration
tests.
