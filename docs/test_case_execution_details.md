# HMS Assigned Test Case Execution Details

Tester: Dhruv Swatantramath (PES1UG24CS155) · Execution date: 2026-11-15 · Tested revision: HEAD `010bdf2849c73f0ad26be266f64e6a57d213bef6`, branch `feature/HMS-F-060-user-account-management` · Build: fresh Stage 3 Windows MSVC/Ninja build with OpenSSL applink; TLS 1.2/1.3 and authenticated API checks succeeded. See the [main execution log](test_execution_log.md) and [environment guide](test_environment_and_reproduction.md).

Appendix A is the procedure authority. Where an assigned procedure requires live roles, patient/billing records, API requests, database inspection, export, platform matrix, long monitoring, or user participants, an unrelated component test is not treated as that procedure.

## TC-APT-05 — Doctor Appointment Schedule

- **Requirement:** HMS-F-024. **Status:** FAIL.
- **Preconditions:** Dr. Rao session; same-day appointments entered out of time order for Dr. Rao and another doctor.
- **Steps/test data:** Log in as Dr. Rao; open My Schedule; inspect a day with multiple out-of-order appointments and another doctor's appointment.
- **Expected:** Own appointments only, sorted by start time.
- **Actual:** Created appointments for two doctors out of insertion-time order. The assigned `/api/v1/doctors/me/schedule` returned 404 and the alternate `/api/v1/doctors/me/appointments` returned 501, so schedule contents, sorting, and ownership could not be verified.
- **Evidence:** [case note](../evidence/APT/TC-APT-05/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Provision test accounts/data, rebuild, call the doctor schedule API/UI and verify ordering and exclusion of the other doctor.

## TC-CLN-05 — Patient Views Own Records and Bills

- **Requirement:** HMS-F-034. **Status:** FAIL.
- **Preconditions:** Patient Asha Nair with two visits and one bill; another patient with separate records; patient token.
- **Steps/test data:** Open own history and bills; check visits, prescriptions, bills and newest-first ordering; request `GET /api/v1/patients/{another_patient_id}/history` with Asha's token.
- **Expected:** Only own records shown, newest first; other-patient request returns HTTP 403 `Access denied`.
- **Actual:** Two synthetic patient profiles and one visit were created. The patient own-history and own-bills endpoints both returned 404; cross-patient record/history requests returned 403. The required history/bills response was unavailable.
- **Evidence:** [case note](../evidence/CLINICAL/TC-CLN-05/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run the full API/UI and database ownership flow with isolated dummy patients and verify record ordering and exact denial response.

## TC-BIL-01 — Generate Itemised Bill

- **Requirement:** HMS-F-040. **Status:** FAIL.
- **Preconditions:** Billing Officer session; completed visit; tax configured at 5%.
- **Steps/test data:** Create bill with Consultation 500.00 and Procedure 300.00.
- **Expected:** Subtotal 800.00, tax 40.00, total 840.00; correct persisted line items and totals.
- **Actual:** A synthetic visit was created and a Billing Officer authenticated. `POST /api/v1/bills` returned 501; no bill or item totals were created.
- **Evidence:** [case note](../evidence/BILLING/TC-BIL-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Build the billing implementation from the correct revision; create a completed dummy visit; exercise API and verify persisted line items and arithmetic in the isolated database.

## TC-BIL-02 — Record Payments and Update Bill Status

- **Requirement:** HMS-F-041. **Status:** BLOCKED.
- **Preconditions:** Unpaid 840.00 bill from TC-BIL-01.
- **Steps/test data:** Pay 400 by UPI; attempt 500; then pay remaining 440.
- **Expected:** First payment accepted, partially paid, balance 440; overpayment rejected; final payment accepted, paid, balance zero; persisted transaction results.
- **Actual:** TC-BIL-01 produced no bill because the bill endpoint returned 501. A payment request also returned 501, but the required unpaid 840.00 bill was absent, so partial payment, overpayment rejection, and final balance were not testable.
- **Evidence:** [case note](../evidence/BILLING/TC-BIL-02/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Execute all three operations against the isolated bill/database and verify payment rows, status, balance and transaction rollback on overpayment.

## TC-BIL-03 — Generate Printable Receipt

- **Requirement:** HMS-F-042. **Status:** BLOCKED.
- **Preconditions:** Payment from TC-BIL-02 exists.
- **Steps/test data:** Generate and open/export the receipt after payment.
- **Expected:** Printable output with unique receipt number, amount, payment mode and date.
- **Actual:** No bill or successful payment existed because the preceding billing/payment endpoints returned 501. Receipt generation returned 501; printable output and receipt fields could not be verified.
- **Evidence:** [case note](../evidence/BILLING/TC-BIL-03/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Complete TC-BIL-02, generate the receipt through the application, open the actual output and verify each field.

## TC-RPT-01 — Daily Appointment and Revenue Reports

- **Requirement:** HMS-F-050. **Status:** FAIL.
- **Preconditions:** Administrator session; payments from the last seven days.
- **Steps/test data:** Generate the seven-day revenue report; export CSV and PDF; open both and compare totals with recorded payments.
- **Expected:** Totals reconcile; both exports are valid and open.
- **Actual:** Authenticated Administrator request to the revenue report endpoint returned 501. No CSV/PDF export or reconciliation was produced.
- **Evidence:** [case note](../evidence/REPORTS/TC-RPT-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Populate isolated payment data, execute the administrator report flow, open the actual files and reconcile amounts.

## TC-ADM-01 — User Management

- **Requirement:** HMS-F-060. **Status:** PASS.
- **Preconditions:** Administrator session.
- **Steps/test data:** Create `nurse1` with Receptionist role; deactivate; attempt login; verify persisted account and administrator-only permissions.
- **Expected:** Correct role stored, deactivation persisted, subsequent login refused, non-admin management denied.
- **Actual:** Admin API created a Receptionist (201), returned the assigned role, deactivated the account (200), and the inactive login returned 401. Database verification showed active=false. Receptionist user creation returned 403.
- **Evidence:** [case note](../evidence/ADMIN/TC-ADM-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run role assignment and deactivation through the administrator API/UI with database verification and a non-admin negative request.

## TC-ADM-02 — Audit Log Filtering

- **Requirement:** HMS-F-061. **Status:** FAIL.
- **Preconditions:** Administrator session; today's audit events for `dr.rao`.
- **Steps/test data:** Filter audit log by user and today's date; inspect user, action, record ID and timestamp; verify non-admin denial.
- **Expected:** Only matching entries with required fields; non-admin cannot read audit log.
- **Actual:** Admin audit filtering returned 501. A non-admin audit read returned 403. The filter result and required fields could not be obtained.
- **Evidence:** [case note](../evidence/ADMIN/TC-ADM-02/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Create controlled audit entries, call administrator and non-administrator endpoints, verify filter results and fields.

## TC-PORT-01 — Platform Compatibility

- **Requirement:** HMS-NF-007. **Status:** PARTIAL.
- **Preconditions:** Windows 11 and Ubuntu 22.04 build environments with required dependencies.
- **Steps/test data:** Build with documented MSVC on Windows and GCC 11 on Ubuntu; run smoke tests on both.
- **Expected:** Both builds and smoke suites pass.
- **Actual:** Fresh current-worktree Windows build succeeded; CTest and PostgreSQL integration CTest each passed 7/7. TLS/API smoke checks succeeded. Ubuntu 22.04 was unavailable: WSL returned E_ACCESSDENIED and no Ubuntu app was installed.
- **Evidence:** [case note](../evidence/PORTABILITY/TC-PORT-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_fresh_windows_build_test_20261115.log).
- **Remaining:** Provision MSVC and Ubuntu 22.04/GCC 11 environments, rebuild both from the same revision and run application smoke tests on each.

## TC-MNT-01 — Unit Test Coverage

- **Requirement:** HMS-NF-008. **Status:** BLOCKED.
- **Preconditions:** Unit tests for all service modules; gcov/lcov tooling.
- **Steps/test data:** Run the suite; capture coverage; inspect line coverage for each service module.
- **Expected:** At least 70% line coverage for each module individually.
- **Actual:** No coverage-instrumented build, gcov/lcov capture, or per-module coverage report was generated. Six prebuilt test executables passing is not coverage evidence.
- **Evidence:** [case note](../evidence/MAINTAINABILITY/TC-MNT-01/status.txt).
- **Remaining:** Build with coverage flags, run all service tests, generate LCOV and report each service module separately.

## TC-SEC-01 — Password Security

- **Requirement:** HMS-SEC-001. **Status:** PASS.
- **Preconditions:** Administrator session and credentials-table access.
- **Steps/test data:** Try creating a user with `abc`; inspect credentials storage for salted hashes and absence of plaintext.
- **Expected:** Weak password rejected; database stores salted hashes only.
- **Actual:** Weak-password user creation returned 400. Isolated database inspection showed 7 tested users, 7 distinct Argon2id salts, and Argon2id-formatted password hashes; no password/hash values were recorded.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Exercise weak-password rejection through the server and inspect dummy credentials rows for encoded salted hashes and no plaintext.

## TC-SEC-02 — Transport Security

- **Requirement:** HMS-SEC-002. **Status:** PASS.
- **Preconditions:** Running server with TLS; testssl.sh or equivalent scanner.
- **Steps/test data:** Attempt plain HTTP; attempt a TLS 1.2 handshake; the server was configured with the repository localhost certificate and bound to loopback.
- **Expected:** HTTP refused; TLS 1.2+ accepted; weak configurations identified.
- **Actual:** Verified TLS 1.2 and TLS 1.3 returned HTTP 200 with AES-GCM ciphers. TLS 1.0/1.1 and plaintext HTTP were rejected; the untrusted self-signed certificate failed verification (result 18). Server stayed running through the negative checks.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-02/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run the broader protocol/cipher scan if required by the full procedure.

## TC-SEC-03 — Session Management

- **Requirement:** HMS-SEC-003. **Status:** PASS.
- **Preconditions:** Valid user session.
- **Steps/test data:** Obtain token; idle ten minutes; call authenticated API; log out; replay token.
- **Expected:** Ten-minute idle expiry and logged-out token both return HTTP 401.
- **Actual:** Logout token replay returned 401. A separate token left unused for 606 seconds returned 401 on its first authenticated request; server remained alive.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-03/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_idle_session_20261115.txt).
- **Remaining:** Repeat the idle-expiry flow on any required target platform or deployment configuration.

## TC-SEC-04 — Input Validation and Injection

- **Requirement:** HMS-SEC-004. **Status:** PARTIAL.
- **Preconditions:** Controlled test database and access to relevant API endpoints.
- **Steps/test data:** Submit `' OR '1'='1` to login/search, `<script>alert(1)</script>` to notes, and invalid/oversized values; inspect responses and database effects.
- **Expected:** Rejection or inert text, no data leak or script execution, parameterised SQL and server-side validation.
- **Actual:** SQL-injection login returned 401; patient search returned 200 with zero matches for the injection string. A `<script>` notes payload was stored only in encrypted visit fields. Oversized/boundary and browser rendering checks were not completed.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-04/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Execute the payload and boundary checks in a controlled disposable environment; inspect response and database effects.

## TC-SEC-05 — Audit Logging

- **Requirements:** HMS-SEC-005 and HMS-SEC-008. **Status:** FAIL.
- **Preconditions:** Audit logging enabled; non-admin account exists.
- **Steps/test data:** Login, view and update a record; inspect audit/application logs for completeness and sensitive content; attempt audit read as non-admin.
- **Expected:** Relevant events logged; no passwords or clinical content logged; audit reads restricted to Administrator.
- **Actual:** Database audit rows recorded login, user creation/deactivation, visit creation and patient updates; non-admin audit access returned 403 and no script note appeared in audit record IDs. Direct patient-record views do not emit a VIEW_PATIENT audit event in this server path, so audit completeness failed.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-05/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Exercise view/update actions in a test environment, inspect real logs for completeness and redaction, and verify non-admin denial.

## TC-SEC-06 — Data Protection at Rest

- **Requirement:** HMS-SEC-006. **Status:** PASS.
- **Preconditions:** SQL access to disposable test database populated with synthetic patient data.
- **Steps/test data:** Query patient-name and diagnosis columns directly; inspect stored values and encryption configuration.
- **Expected:** Sensitive values are ciphertext, not readable plaintext.
- **Actual:** SQL checks against the isolated database confirmed the updated synthetic patient name, visit diagnosis, and script notes were absent as plaintext from stored BYTEA fields.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-06/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Insert synthetic patient/diagnosis data, inspect raw stored values and verify encryption/key handling.

## TC-SEC-07 — Server-Side Authorisation

- **Requirement:** HMS-SEC-007. **Status:** PASS.
- **Preconditions:** Patient session and a second patient's record.
- **Steps/test data:** Request `GET /api/v1/patients/{another_patient_id}` with the first patient's valid token.
- **Expected:** HTTP 403 with no other-patient data returned.
- **Actual:** Patient A could read its own record (200). Requests for Patient B record and history returned 403; no protected record body was returned.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-07/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run the direct cross-patient API request with a valid patient token and verify HTTP 403 and empty/no protected payload.

## TC-AVL-01 — Availability

- **Requirement:** HMS-NF-004. **Status:** BLOCKED.
- **Preconditions:** Staging service and minute-by-minute health monitor.
- **Steps/test data:** Monitor seven days, 06:00–22:00; compute measured availability.
- **Expected:** At least 99.5% availability.
- **Actual:** Liveness and readiness returned 200 during a short TLS smoke check, but no minute-level sustained monitor or complete seven-day observation was run.
- **Evidence:** [case note](../evidence/AVAILABILITY/TC-AVL-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Provision and monitor staging for the complete seven-day period and calculate availability from timestamped checks.

## TC-REL-01 — Backup and Recovery

- **Requirement:** HMS-NF-006. **Status:** BLOCKED.
- **Preconditions:** Backup job and populated disposable test database.
- **Steps/test data:** Trigger backup; verify creation/encryption; restore in isolated environment; validate data and measure duration/data loss.
- **Expected:** Encrypted daily backup, no more than 24 hours of data loss and restore within four hours.
- **Actual:** No backup job, disposable database, encrypted backup, or restore drill was executed. No shared or production database was modified.
- **Evidence:** [case note](../evidence/RELIABILITY/TC-REL-01/status.txt).
- **Remaining:** Configure isolated test backup and perform the timed encrypted restore drill with synthetic data.

## TC-USA-01 — Usability

- **Requirements:** HMS-NF-005 and HMS-NF-009. **Status:** BLOCKED.
- **Preconditions:** Five trained receptionists, prepared test data and task sheet.
- **Steps/test data:** Each participant registers a patient and books an appointment; measure time and help needed.
- **Expected:** Registration in three minutes or less and at least 90% of core tasks without help.
- **Actual:** No participant sessions were conducted. No participant counts, task timing, or help observations were invented.
- **Evidence:** [case note](../evidence/USABILITY/TC-USA-01/status.txt).
- **Remaining:** Recruit five trained participants and execute/document the defined protocol against a working test system.
