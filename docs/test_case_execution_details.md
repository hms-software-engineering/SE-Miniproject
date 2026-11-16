# HMS Assigned Test Case Execution Details

Tester: Dhruv Swatantramath (PES1UG24CS155) ? Execution date: 2026-11-16 ? Tested revision: HEAD `010bdf2849c73f0ad26be266f64e6a57d213bef6` plus pre-existing dirty worktree and Stage 6 edits. Fresh Stage 6 Windows clang-cl/Ninja coverage build compiled successfully; normal and PostgreSQL-enabled CTest each passed 8/8. Stage 5 and Stage 6 API and artifact checks ran against isolated PostgreSQL 17.11. See the [main execution log](test_execution_log.md) and [environment guide](test_environment_and_reproduction.md).

Appendix A is the procedure authority. Where an assigned procedure requires live roles, patient/billing records, API requests, database inspection, export, platform matrix, long monitoring, or user participants, an unrelated component test is not treated as that procedure.

## TC-APT-05 â€” Doctor Appointment Schedule

- **Requirement:** HMS-F-024. **Status:** PASS.
- **Preconditions:** Dr. Rao session; same-day appointments entered out of time order for Dr. Rao and another doctor.
- **Steps/test data:** Log in as Dr. Rao; open My Schedule; inspect a day with multiple out-of-order appointments and another doctor's appointment.
- **Expected:** Own appointments only, sorted by start time.
- **Actual:** The assigned endpoint returned 200 for a Doctor and 403 for a non-doctor. Three appointments inserted at 09:00, 11:00, and 10:00 returned in 09:00, 10:00, 11:00 order, all with the authenticated doctor's ID; a same-day appointment for another doctor was excluded. The in-memory test also checks date/doctor filtering and sorting.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** The desktop My Schedule UI was not opened; the API case procedure passed.

## TC-CLN-05 â€” Patient Views Own Records and Bills

- **Requirement:** HMS-F-034. **Status:** PASS.
- **Preconditions:** Patient Asha Nair with two visits and one bill; another patient with separate records; patient token.
- **Steps/test data:** Open own history and bills; check visits, prescriptions, bills and newest-first ordering; request `GET /api/v1/patients/{another_patient_id}/history` with Asha's token.
- **Expected:** Only own records shown, newest first; other-patient request returns HTTP 403 `Access denied`.
- **Actual:** The authenticated patient history response returned two clinical visits newest first, including prescriptions. Its bills response included the patient-owned bill; a second patient bills response was empty, and cross-patient history returned 403. Clinical text was returned through the existing authenticated AES-GCM decryption path.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** The desktop patient UI was not opened; API ownership and ordering checks passed.

## TC-BIL-01 â€” Generate Itemised Bill

- **Requirement:** HMS-F-040. **Status:** PASS.
- **Preconditions:** Billing Officer session; completed visit; tax configured at 5%.
- **Steps/test data:** Create bill with Consultation 500.00 and Procedure 300.00.
- **Expected:** Subtotal 800.00, tax 40.00, total 840.00; correct persisted line items and totals.
- **Actual:** POST /api/v1/bills returned 201 for Consultation 500.00 and Procedure 300.00. PostgreSQL contained two line items totaling 800.00, subtotal 800.00, 5% tax 40.00, and total 840.00. Invalid negative/blank input returned 400; a patient request was denied with 403.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** None for the API and database procedure.

## TC-BIL-02 â€” Record Payments and Update Bill Status

- **Requirement:** HMS-F-041. **Status:** PASS.
- **Preconditions:** Unpaid 840.00 bill from TC-BIL-01.
- **Steps/test data:** Pay 400 by UPI; attempt 500; then pay remaining 440.
- **Expected:** First payment accepted, partially paid, balance 440; overpayment rejected; final payment accepted, paid, balance zero; persisted transaction results.
- **Actual:** A 400.00 UPI payment returned 201 and left a 440.00 balance with Partially Paid status. A 500.00 overpayment returned 409 and did not create a payment row. The final 440.00 payment returned 201 with zero balance and Paid status. PostgreSQL showed exactly two payment rows totaling 840.00. Zero and unauthorized payment requests returned 400 and 403.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** None for the assigned payment and balance procedure.

## TC-BIL-03 â€” Generate Printable Receipt

- **Requirement:** HMS-F-042. **Status:** PASS.
- **Preconditions:** Payment from TC-BIL-02 exists.
- **Steps/test data:** Generate and open/export the receipt after payment.
- **Expected:** Printable output with unique receipt number, amount, payment mode and date.
- **Actual:** After the 400.00 UPI partial payment, the receipt endpoint returned printable HTML with a unique persisted receipt number, payment amount 400.00, UPI mode, and payment date. The HTML was opened in Edge and visually inspected; Edge printed it to PDF, which was rendered and visually checked as a readable one-page receipt with all required fields. Two persisted receipts had two distinct receipt numbers.
- **Evidence:** [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt), [generated HTML](../evidence/automated_tests/stage5_generated_receipt_20261115.html), [browser render](../evidence/automated_tests/stage5_receipt_render_final_20261115.png), [printed PDF](../evidence/automated_tests/stage5_receipt_print_output_20261115.pdf), [PDF render](../evidence/automated_tests/stage5_receipt_print_render_20261115.png).
- **Remaining:** None for the receipt procedure exercised.

## TC-RPT-01 â€” Daily Appointment and Revenue Reports

- **Requirement:** HMS-F-050. **Status:** PASS.
- **Preconditions:** Administrator session; payments from the last seven days.
- **Steps/test data:** Generate the seven-day revenue report; export CSV and PDF; open both and compare totals with recorded payments.
- **Expected:** Totals reconcile; both exports are valid and open.
- **Actual:** The Admin seven-day revenue report returned 200 for the preceding seven calendar days including today. Its CSV total reconciled with PostgreSQL payments and opened in Excel (9 rows, 2 columns); the PDF was rendered and visually inspected with seven daily rows and the same displayed total. The seven-day output was 12,160.00 in this isolated test database. The SRS daily appointment export was also implemented and exercised separately: CSV fields/data and PDF response were checked, a non-admin was denied (403), and invalid date was rejected (400). Appendix A TC-RPT-01 procedure covers the revenue export; the daily appointment report is a separate SRS feature.
- **Evidence:** [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt), [revenue CSV](../evidence/automated_tests/stage5_revenue_report_20261115.csv), [revenue PDF](../evidence/automated_tests/stage5_revenue_report_20261115.pdf), [verified PDF render](../evidence/automated_tests/stage5_revenue_pdf_render_verified_20261115.png), [daily appointment CSV](../evidence/automated_tests/stage5_daily_appointment_report_2026-12-21.csv), [daily appointment PDF](../evidence/automated_tests/stage5_daily_appointment_report_2026-12-21.pdf).
- **Remaining:** None for the Appendix A revenue-report procedure exercised.

## TC-ADM-01 â€” User Management

- **Requirement:** HMS-F-060. **Status:** PASS.
- **Preconditions:** Administrator session.
- **Steps/test data:** Create `nurse1` with Receptionist role; deactivate; attempt login; verify persisted account and administrator-only permissions.
- **Expected:** Correct role stored, deactivation persisted, subsequent login refused, non-admin management denied.
- **Actual:** Admin API created a Receptionist (201), returned the assigned role, deactivated the account (200), and the inactive login returned 401. Database verification showed active=false. Receptionist user creation returned 403.
- **Evidence:** [case note](../evidence/ADMIN/TC-ADM-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run role assignment and deactivation through the administrator API/UI with database verification and a non-admin negative request.

## TC-ADM-02 â€” Audit Log Filtering

- **Requirement:** HMS-F-061. **Status:** PASS.
- **Preconditions:** Administrator session; today's audit events for `dr.rao`.
- **Steps/test data:** Filter audit log by user and today's date; inspect user, action, record ID and timestamp; verify non-admin denial.
- **Expected:** Only matching entries with required fields; non-admin cannot read audit log.
- **Actual:** The Administrator filter returned four rows for the generated username and requested date; every row matched both and included user, action, record ID, and timestamp. A non-administrator audit request returned 403.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** None for the administrator API filter procedure.

## TC-PORT-01 â€” Platform Compatibility

- **Requirement:** HMS-NF-007. **Status:** PARTIAL.
- **Preconditions:** Windows 11 and Ubuntu 22.04 build environments with required dependencies.
- **Steps/test data:** Build with documented MSVC on Windows and GCC 11 on Ubuntu; run smoke tests on both.
- **Expected:** Both builds and smoke suites pass.
- **Actual:** Fresh Stage 5 current-worktree Windows clang-cl/Ninja build succeeded; CTest and PostgreSQL integration CTest each passed 8/8. TLS/API smoke checks succeeded. Ubuntu 22.04 was unavailable: WSL returned E_ACCESSDENIED and no Ubuntu app was installed.
- **Evidence:** [case note](../evidence/PORTABILITY/TC-PORT-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_fresh_windows_build_test_20261115.log).
- **Remaining:** Provision MSVC and Ubuntu 22.04/GCC 11 environments, rebuild both from the same revision and run application smoke tests on each.

## TC-MNT-01 â€” Unit Test Coverage

- **Requirement:** HMS-NF-008. **Status:** PASS (Stage 8; prior Stage 5/6 FAIL results are superseded by the newer verified measurement).
- **Preconditions:** Unit tests for all service modules; gcov/lcov tooling.
- **Steps/test data:** Run the suite; capture coverage; inspect line coverage for each service module.
- **Expected:** At least 70% line coverage for each module individually.
- **Actual:** A fresh clang-cl coverage-instrumented build completed; ordinary and PostgreSQL-enabled CTest each passed 8/8. LLVM per-executable profiles were merged and source-only line coverage was measured. Results: auth 75.19%, patient modules combined 80.16%, health (availability + health service) 84.91%, password security 91.55%, appointments 61.46%, API 57.78%, billing 37.14%, and clinical 6.70%. Thus the per-module >=70% acceptance criterion is not met; the procedure ran and is classified FAIL. The API case runner was not included in the module profile because the instrumented server was forcibly stopped and did not flush its profile; do not interpret the module figures as API workflow coverage.
- **Evidence:** [Stage 6 coverage case note](../evidence/MAINTAINABILITY/TC-MNT-01/status.txt), [Stage 6 module report](../evidence/automated_tests/stage6_20261115/coverage_per_module.txt), [auth](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_auth_coverage_20261115.txt), [patients](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_patients_coverage_20261115.txt), [appointments](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_appointments_coverage_20261115.txt), [API](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_api_coverage_20261115.txt), [billing](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_billing_coverage_20261115.txt), [clinical](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_clinical_coverage_20261115.txt), [health](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_health_coverage_20261115.txt), [password](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_password_coverage_20261115.txt).
- **Remaining:** Increase appointment, API, billing, and clinical source line coverage to at least 70%, instrument the API workflow if applicable, then rerun and report.

## TC-SEC-01 â€” Password Security

- **Requirement:** HMS-SEC-001. **Status:** PASS.
- **Preconditions:** Administrator session and credentials-table access.
- **Steps/test data:** Try creating a user with `abc`; inspect credentials storage for salted hashes and absence of plaintext.
- **Expected:** Weak password rejected; database stores salted hashes only.
- **Actual:** Weak-password user creation returned 400. Isolated database inspection showed 7 tested users, 7 distinct Argon2id salts, and Argon2id-formatted password hashes; no password/hash values were recorded.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Exercise weak-password rejection through the server and inspect dummy credentials rows for encoded salted hashes and no plaintext.

## TC-SEC-02 â€” Transport Security

- **Requirement:** HMS-SEC-002. **Status:** PASS.
- **Preconditions:** Running server with TLS; testssl.sh or equivalent scanner.
- **Steps/test data:** Attempt plain HTTP; attempt a TLS 1.2 handshake; the server was configured with the repository localhost certificate and bound to loopback.
- **Expected:** HTTP refused; TLS 1.2+ accepted; weak configurations identified.
- **Actual:** Verified TLS 1.2 and TLS 1.3 returned HTTP 200 with AES-GCM ciphers. TLS 1.0/1.1 and plaintext HTTP were rejected; the untrusted self-signed certificate failed verification (result 18). Server stayed running through the negative checks.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-02/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run the broader protocol/cipher scan if required by the full procedure.

## TC-SEC-03 â€” Session Management

- **Requirement:** HMS-SEC-003. **Status:** PASS.
- **Preconditions:** Valid user session.
- **Steps/test data:** Obtain token; idle ten minutes; call authenticated API; log out; replay token.
- **Expected:** Ten-minute idle expiry and logged-out token both return HTTP 401.
- **Actual:** Logout token replay returned 401. A separate token left unused for 606 seconds returned 401 on its first authenticated request; server remained alive.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-03/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_idle_session_20261115.txt).
- **Remaining:** Repeat the idle-expiry flow on any required target platform or deployment configuration.

## TC-SEC-04 â€” Input Validation and Injection

- **Requirement:** HMS-SEC-004. **Status:** PARTIAL.
- **Preconditions:** Controlled test database and access to relevant API endpoints.
- **Steps/test data:** Submit `' OR '1'='1` to login/search, `<script>alert(1)</script>` to notes, and invalid/oversized values; inspect responses and database effects.
- **Expected:** Rejection or inert text, no data leak or script execution, parameterised SQL and server-side validation.
- **Actual:** Login SQL-injection returned 401; patient search returned zero matches. A `<script>alert(1)</script>` notes payload was returned as JSON data and stored only in encrypted visit fields. An 8,001-character note returned 400; an 8,000-character note returned 201 and persisted exactly. Automated HTML escaping checks passed. Rendering in the actual HMS desktop UI was not completed (Qt6 client unavailable), and ZAP/sqlmap were not run, so the full case remains PARTIAL.
- **Evidence:** [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt).
- **Remaining:** Verify payload behavior in the actual UI/browser and complete any required security scanner procedure.

## TC-SEC-05 â€” Audit Logging

- **Requirements:** HMS-SEC-005 and HMS-SEC-008. **Status:** PASS.
- **Preconditions:** Audit logging enabled; non-admin account exists.
- **Steps/test data:** Login, view and update a record; inspect audit/application logs for completeness and sensitive content; attempt audit read as non-admin.
- **Expected:** Relevant events logged; no passwords or clinical content logged; audit reads restricted to Administrator.
- **Actual:** A successful direct patient-record GET wrote exactly one VIEW_PATIENT audit row. Patient update and visit events were present, no test clinical note appeared in audit IDs, and non-administrator audit reads returned 403.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** None for the direct-view audit event and role-denial checks.

## TC-SEC-06 â€” Data Protection at Rest

- **Requirement:** HMS-SEC-006. **Status:** PASS.
- **Preconditions:** SQL access to disposable test database populated with synthetic patient data.
- **Steps/test data:** Query patient-name and diagnosis columns directly; inspect stored values and encryption configuration.
- **Expected:** Sensitive values are ciphertext, not readable plaintext.
- **Actual:** SQL checks against the isolated database confirmed the updated synthetic patient name, visit diagnosis, and script notes were absent as plaintext from stored BYTEA fields.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-06/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Insert synthetic patient/diagnosis data, inspect raw stored values and verify encryption/key handling.

## TC-SEC-07 â€” Server-Side Authorisation

- **Requirement:** HMS-SEC-007. **Status:** PASS.
- **Preconditions:** Patient session and a second patient's record.
- **Steps/test data:** Request `GET /api/v1/patients/{another_patient_id}` with the first patient's valid token.
- **Expected:** HTTP 403 with no other-patient data returned.
- **Actual:** Patient A could read its own record (200). Requests for Patient B record and history returned 403; no protected record body was returned.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-07/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run the direct cross-patient API request with a valid patient token and verify HTTP 403 and empty/no protected payload.

## TC-AVL-01 â€” Availability

- **Requirement:** HMS-NF-004. **Status:** BLOCKED.
- **Preconditions:** Staging service and minute-by-minute health monitor.
- **Steps/test data:** Monitor seven days, 06:00â€“22:00; compute measured availability.
- **Expected:** At least 99.5% availability.
- **Actual:** Liveness and readiness returned 200 during a short TLS smoke check, but no minute-level sustained monitor or complete seven-day observation was run.
- **Evidence:** [case note](../evidence/AVAILABILITY/TC-AVL-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Provision and monitor staging for the complete seven-day period and calculate availability from timestamped checks.

## TC-REL-01 â€” Backup and Recovery

- **Requirement:** HMS-NF-006. **Status:** BLOCKED.
- **Preconditions:** Backup job and populated disposable test database.
- **Steps/test data:** Trigger backup; verify creation/encryption; restore in isolated environment; validate data and measure duration/data loss.
- **Expected:** Encrypted daily backup, no more than 24 hours of data loss and restore within four hours.
- **Actual:** A manual isolated PostgreSQL 17 synthetic-data backup/restore drill passed: custom-format dump, DPAPI encryption, SHA256 recorded, source DB dropped, restored fingerprint matched with zero lost rows, and restore took 2.088 seconds. The required daily scheduled backup job is not configured, so the full assigned procedure remains blocked despite the successful manual restore drill.
- **Evidence:** [case note](../evidence/RELIABILITY/TC-REL-01/status.txt).
- **Remaining:** Configure isolated test backup and perform the timed encrypted restore drill with synthetic data.

## TC-USA-01 â€” Usability

- **Requirements:** HMS-NF-005 and HMS-NF-009. **Status:** BLOCKED.
- **Preconditions:** Five trained receptionists, prepared test data and task sheet.
- **Steps/test data:** Each participant registers a patient and books an appointment; measure time and help needed.
- **Expected:** Registration in three minutes or less and at least 90% of core tasks without help.
- **Actual:** No participant sessions were conducted. No participant counts, task timing, or help observations were invented.
- **Evidence:** [case note](../evidence/USABILITY/TC-USA-01/status.txt).
- **Remaining:** Recruit five trained participants and execute/document the defined protocol against a working test system.

## Stage 6 recheck and coverage update

The 14 cases retain their previously completed full-procedure evidence; Stage 6 reran the relevant server/API regression paths for appointments, clinical history/bills, billing, payment, receipts, revenue exports, audit filtering, password/authentication, TLS, patient-view auditing, encryption at rest, authorization, and logout replay. This regression run is supporting evidence and does not replace UI, platform, coverage, elapsed-time, or participant procedures. The final case status table is 14 PASS, 1 FAIL, 3 BLOCKED, 2 PARTIAL. Stage 6 API evidence: [API execution](../evidence/automated_tests/stage6_20261115/stage6_api_execution.txt).

Coverage was refreshed from the fresh Stage 6 build and unit/PostgreSQL CTest profiles. Line coverage: auth 75.19%; patients 80.16%; health 84.91%; password security 91.55%; appointments 61.46%; billing 37.14%; clinical 6.70%. Route coverage for src/main.cpp remains last measured at 57.78%; the instrumented API process was terminated by the existing runner before profile flush. The 70% per-module target remains unmet. See [Stage 6 coverage](../evidence/automated_tests/stage6_20261115/coverage_per_module.txt).

TC-REL-01 remains BLOCKED: isolated encrypted backup/restore and failure behavior passed, but no daily task was configured against an HMS staging database. TC-AVL-01 remains BLOCKED pending seven full days with a staging HTTPS endpoint; monitoring scripts are prepared but have not been started. TC-USA-01 remains BLOCKED pending five actual trained receptionists. TC-PORT-01 remains PARTIAL pending official Ubuntu 22.04/GCC 11 and MSVC 2019 builds. TC-SEC-04 remains PARTIAL because Qt6 was unavailable and no dynamic scanner was installed; API XSS and SQL injection checks and clang-tidy static security checks were recorded.

## Stage 7 recheck (2026-11-16)

No application logic or test procedures changed in this recheck, so Stage 6 remains the latest verified assigned-case result: 14 PASS, 1 FAIL, 3 BLOCKED, and 2 PARTIAL (70%). No case status is promoted. The Stage 7 machine check confirmed Qt6 is unavailable, WSL cannot start because virtualization prerequisites are unavailable, and the existing CI Windows job uses MinGW rather than MSVC 2019. The CI workflow was not triggered because the tested dirty source state is not pushed. Clinical coverage remains substantially below threshold (6.70%), so no low-value mock assertions were added. The present blockers and required external inputs are recorded in [Stage 7 status recheck](../evidence/automated_tests/stage7/status_recheck.txt).

## TC-MNT-01 Stage 8 completion

A fresh Clang-cl 23.1.5/Ninja coverage build succeeded. Normal CTest passed 8/8. The isolated PostgreSQL integration run passed the clinical, appointment, and billing service tests (3/3). The complete assigned API regression passed all required assertions and the server shut down gracefully, flushing its LLVM profile. Merged profiles from successful tests only were used; unsuccessful API harness attempts were excluded. Source-only line coverage: API routes 72.28%; appointments 94.31%; billing 98.21%; clinical 88.47%; auth service 79.52%; patient service 87.47%; PostgreSQL patient repository 83.17%; health service 100%; availability 82.80%; password security 91.55%; RBAC 95.58%. Every measured applicable service/API module meets the official >=70% threshold, so TC-MNT-01 is PASS. Detailed report: [Stage 8 module coverage](../evidence/automated_tests/stage8/coverage_per_module.txt); test evidence: [fresh build](../evidence/automated_tests/stage8/fresh_coverage_build.log), [CTest](../evidence/automated_tests/stage8/fresh_coverage_ctest.log), [PostgreSQL service integration](../evidence/automated_tests/stage8/clinical_postgresql_integration.log), and [API regression](../evidence/automated_tests/stage8/api_execution.txt).

## Stage 8 assigned-case status recheck (2026-11-16)

Latest case counts: **15 PASS, 0 FAIL, 3 BLOCKED, 2 PARTIAL (75%)**. TC-MNT-01 is the one newly promoted case. The Stage 8 API run provided additional regression evidence for the already passing schedule, patient-history, billing, payments, receipts, revenue exports, audit, security, TLS and authentication workflows; it does not replace unavailable official procedures for the remaining partial/blocked cases. TC-PORT-01 and TC-SEC-04 remain PARTIAL; TC-REL-01, TC-AVL-01 and TC-USA-01 remain BLOCKED. No other case status changed.
## Stage 9 remaining-case recheck

- **TC-PORT-01 — PARTIAL:** No Windows 11/MSVC 2019 v142 build or Ubuntu 22.04/GCC 11 run occurred. The installed host toolset is Visual Studio Build Tools 18/MSVC 14.51; WSL virtualization prerequisites are disabled. Existing CI uses Ubuntu 22.04 and Windows MinGW, and was not triggered on the dirty local worktree.
- **TC-SEC-04 — PARTIAL:** Qt6 was unavailable; the real desktop client was not built or launched, and no rendering screenshots were created. Stage 8 API XSS/input checks remain supporting evidence only.
- **TC-REL-01 — BLOCKED:** No staging PostgreSQL configuration, approved separate backup target, external protected `pgpass`, or designated operating account was provided. No scheduled real backup or restore was performed.
- **TC-AVL-01 — BLOCKED:** The monitor now summarizes 6,720 expected checks. Its synthetic calculation test passed at 99.9554%, including outage, missing, duplicate, and out-of-window cases. No trusted staging URL was available and no seven-day observation started.
- **TC-USA-01 — BLOCKED:** The Stage 9 kit has participant instructions, five fictional test records, registration timing, booking results, unaided task, error/assistance, and final-results templates. No actual participants or results exist.

Evidence: [Stage 9 readiness assessment](../evidence/automated_tests/stage9/readiness_assessment.md), [availability calculation test](../evidence/automated_tests/stage9/availability_calculation_test.json.test.txt), and [usability kit](../evidence/automated_tests/stage9/usability/participant_instructions.md). Overall status remains **15 PASS, 0 FAIL, 3 BLOCKED, 2 PARTIAL (75%)**.
## Stage 10 platform/security recheck

- **TC-PORT-01 — PARTIAL:** No official build/smoke test was run. The local MSVC is 14.51, not v142; WSL cannot start Ubuntu; CI's Ubuntu 22.04 job does not include this dirty local worktree, and its Windows job is MSYS2 MinGW. No approved matching Windows runner was exposed by the available read-only connection.
- **TC-SEC-04 — PARTIAL:** CMake requires Qt6 Widgets and Network without a pinned minimum version. These modules are absent and Qt is not in the vcpkg manifest; CMake's Stage 8 configure skipped `hms_client`. Stage 10 did not install an undeclared/unapproved package. The actual desktop client was not built or launched; there are no Stage 10 render results or screenshots. Previous SQL injection/API evidence was reused.

No application code changed, so Stage 8 remains the latest relevant build and regression evidence. No case status was promoted. See [Stage 10 environment and results](../evidence/automated_tests/stage10/environment_and_results.md).