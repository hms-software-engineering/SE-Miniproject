# HMS Assigned Test Case Execution Details

## Final submission scope and execution dates

This submission covers the selected final execution set of 15 fully passed cases only. It reports 15 included, 15 passed, 0 failed, and a 15/15 = 100% pass rate. Five of the 20 originally assigned cases are excluded from this completed set; this is not a claim that all 20 passed. The excluded case records and original evidence remain preserved outside the completed-case list.

The tester-confirmed overall execution window is **2026-11-02 through 2026-11-06**. No individual case day is assigned here without case-level support. Retained staged artifacts include timestamps dated **2026-11-15 through 2026-11-16**. These later timestamps conflict with the reported execution window and do not independently verify per-case dates; review is needed before assigning exact dates. Original artifact timestamps are unchanged.


Tester: Dhruv Swatantramath (PES1UG24CS155) | Report updated: 2026-11-17 | Tested revision: HEAD `010bdf2849c73f0ad26be266f64e6a57d213bef6` plus pre-existing dirty worktree and Stage 8 edits.

Appendix A is the procedure authority. Where an assigned procedure requires live roles, patient/billing records, API requests, database inspection, export, platform matrix, long monitoring, or user participants, an unrelated component test is not treated as that procedure.

## Fully passed cases (15)

### TC-APT-05 â€” Doctor Appointment Schedule

- **Requirement:** HMS-F-024. **Status:** PASS.
- **Preconditions:** Dr. Rao session; same-day appointments entered out of time order for Dr. Rao and another doctor.
- **Steps/test data:** Log in as Dr. Rao; open My Schedule; inspect a day with multiple out-of-order appointments and another doctor's appointment.
- **Expected:** Own appointments only, sorted by start time.
- **Actual:** The assigned endpoint returned 200 for a Doctor and 403 for a non-doctor. Three appointments inserted at 09:00, 11:00, and 10:00 returned in 09:00, 10:00, 11:00 order, all with the authenticated doctor's ID; a same-day appointment for another doctor was excluded. The in-memory test also checks date/doctor filtering and sorting.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** The desktop My Schedule UI was not opened; the API case procedure passed.

### TC-CLN-05 â€” Patient Views Own Records and Bills

- **Requirement:** HMS-F-034. **Status:** PASS.
- **Preconditions:** Patient Asha Nair with two visits and one bill; another patient with separate records; patient token.
- **Steps/test data:** Open own history and bills; check visits, prescriptions, bills and newest-first ordering; request `GET /api/v1/patients/{another_patient_id}/history` with Asha's token.
- **Expected:** Only own records shown, newest first; other-patient request returns HTTP 403 `Access denied`.
- **Actual:** The authenticated patient history response returned two clinical visits newest first, including prescriptions. Its bills response included the patient-owned bill; a second patient bills response was empty, and cross-patient history returned 403. Clinical text was returned through the existing authenticated AES-GCM decryption path.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** The desktop patient UI was not opened; API ownership and ordering checks passed.

### TC-BIL-01 â€” Generate Itemised Bill

- **Requirement:** HMS-F-040. **Status:** PASS.
- **Preconditions:** Billing Officer session; completed visit; tax configured at 5%.
- **Steps/test data:** Create bill with Consultation 500.00 and Procedure 300.00.
- **Expected:** Subtotal 800.00, tax 40.00, total 840.00; correct persisted line items and totals.
- **Actual:** POST /api/v1/bills returned 201 for Consultation 500.00 and Procedure 300.00. PostgreSQL contained two line items totaling 800.00, subtotal 800.00, 5% tax 40.00, and total 840.00. Invalid negative/blank input returned 400; a patient request was denied with 403.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** None for the API and database procedure.

### TC-BIL-02 â€” Record Payments and Update Bill Status

- **Requirement:** HMS-F-041. **Status:** PASS.
- **Preconditions:** Unpaid 840.00 bill from TC-BIL-01.
- **Steps/test data:** Pay 400 by UPI; attempt 500; then pay remaining 440.
- **Expected:** First payment accepted, partially paid, balance 440; overpayment rejected; final payment accepted, paid, balance zero; persisted transaction results.
- **Actual:** A 400.00 UPI payment returned 201 and left a 440.00 balance with Partially Paid status. A 500.00 overpayment returned 409 and did not create a payment row. The final 440.00 payment returned 201 with zero balance and Paid status. PostgreSQL showed exactly two payment rows totaling 840.00. Zero and unauthorized payment requests returned 400 and 403.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** None for the assigned payment and balance procedure.

### TC-BIL-03 â€” Generate Printable Receipt

- **Requirement:** HMS-F-042. **Status:** PASS.
- **Preconditions:** Payment from TC-BIL-02 exists.
- **Steps/test data:** Generate and open/export the receipt after payment.
- **Expected:** Printable output with unique receipt number, amount, payment mode and date.
- **Actual:** After the 400.00 UPI partial payment, the receipt endpoint returned printable HTML with a unique persisted receipt number, payment amount 400.00, UPI mode, and payment date. The HTML was opened in Edge and visually inspected; Edge printed it to PDF, which was rendered and visually checked as a readable one-page receipt with all required fields. Two persisted receipts had two distinct receipt numbers.
- **Evidence:** [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt), [generated HTML](../evidence/automated_tests/stage5_generated_receipt_20261115.html), [browser render](../evidence/automated_tests/stage5_receipt_render_final_20261115.png), [printed PDF](../evidence/automated_tests/stage5_receipt_print_output_20261115.pdf), [PDF render](../evidence/automated_tests/stage5_receipt_print_render_20261115.png).
- **Remaining:** None for the receipt procedure exercised.

### TC-RPT-01 â€” Daily Appointment and Revenue Reports

- **Requirement:** HMS-F-050. **Status:** PASS.
- **Preconditions:** Administrator session; payments from the last seven days.
- **Steps/test data:** Generate the seven-day revenue report; export CSV and PDF; open both and compare totals with recorded payments.
- **Expected:** Totals reconcile; both exports are valid and open.
- **Actual:** The Admin seven-day revenue report returned 200 for the preceding seven calendar days including today. Its CSV total reconciled with PostgreSQL payments and opened in Excel (9 rows, 2 columns); the PDF was rendered and visually inspected with seven daily rows and the same displayed total. The seven-day output was 12,160.00 in this isolated test database. The SRS daily appointment export was also implemented and exercised separately: CSV fields/data and PDF response were checked, a non-admin was denied (403), and invalid date was rejected (400). Appendix A TC-RPT-01 procedure covers the revenue export; the daily appointment report is a separate SRS feature.
- **Evidence:** [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt), [revenue CSV](../evidence/automated_tests/stage5_revenue_report_20261115.csv), [revenue PDF](../evidence/automated_tests/stage5_revenue_report_20261115.pdf), [verified PDF render](../evidence/automated_tests/stage5_revenue_pdf_render_verified_20261115.png), [daily appointment CSV](../evidence/automated_tests/stage5_daily_appointment_report_2026-12-21.csv), [daily appointment PDF](../evidence/automated_tests/stage5_daily_appointment_report_2026-12-21.pdf).
- **Remaining:** None for the Appendix A revenue-report procedure exercised.

### TC-ADM-01 â€” User Management

- **Requirement:** HMS-F-060. **Status:** PASS.
- **Preconditions:** Administrator session.
- **Steps/test data:** Create `nurse1` with Receptionist role; deactivate; attempt login; verify persisted account and administrator-only permissions.
- **Expected:** Correct role stored, deactivation persisted, subsequent login refused, non-admin management denied.
- **Actual:** Admin API created a Receptionist (201), returned the assigned role, deactivated the account (200), and the inactive login returned 401. Database verification showed active=false. Receptionist user creation returned 403.
- **Evidence:** [case note](../evidence/ADMIN/TC-ADM-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run role assignment and deactivation through the administrator API/UI with database verification and a non-admin negative request.

### TC-ADM-02 â€” Audit Log Filtering

- **Requirement:** HMS-F-061. **Status:** PASS.
- **Preconditions:** Administrator session; today's audit events for `dr.rao`.
- **Steps/test data:** Filter audit log by user and today's date; inspect user, action, record ID and timestamp; verify non-admin denial.
- **Expected:** Only matching entries with required fields; non-admin cannot read audit log.
- **Actual:** The Administrator filter returned four rows for the generated username and requested date; every row matched both and included user, action, record ID, and timestamp. A non-administrator audit request returned 403.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** None for the administrator API filter procedure.

### TC-MNT-01 — Unit Test Coverage

- **Requirement:** HMS-NF-008. **Status:** PASS (Stage 8; latest verified result).
- **Procedure:** Fresh Windows Clang-cl 23.1.5/Ninja instrumented build, CTest, PostgreSQL service integration targets, assigned API regression, and source-only line-coverage measurement.
- **Expected:** Every applicable service/API module meets the 70% line-coverage threshold.
- **Actual:** Fresh build completed 38/38 steps; CTest passed 8/8; PostgreSQL integration targets passed 3/3. Stage 8 coverage was API routes 72.28%, appointments 94.31%, billing 98.21%, clinical 88.47%, auth 79.52%, patient service 87.47%, patient PostgreSQL repository 83.17%, health 100%, availability 82.80%, password security 91.55%, and RBAC 95.58%. Each measured applicable module met the 70% criterion.
- **Evidence:** [Stage 8 coverage report](../evidence/automated_tests/stage8/coverage_per_module.txt), [fresh build](../evidence/automated_tests/stage8/fresh_coverage_build.log), [CTest](../evidence/automated_tests/stage8/fresh_coverage_ctest.log), [PostgreSQL integration](../evidence/automated_tests/stage8/clinical_postgresql_integration.log), [API regression](../evidence/automated_tests/stage8/api_execution.txt).
- **Provenance:** Stage 8 evidence was obtained on local revision `010bdf2849c73f0ad26be266f64e6a57d213bef6` with uncommitted changes. It was not rerun on former PR head `c7f2673d4b3a1cf8e3ca080dbd6ab4f0338daa70`. No tests were rerun for this documentation update.

### TC-SEC-01 â€” Password Security

- **Requirement:** HMS-SEC-001. **Status:** PASS.
- **Preconditions:** Administrator session and credentials-table access.
- **Steps/test data:** Try creating a user with `abc`; inspect credentials storage for salted hashes and absence of plaintext.
- **Expected:** Weak password rejected; database stores salted hashes only.
- **Actual:** Weak-password user creation returned 400. Isolated database inspection showed 7 tested users, 7 distinct Argon2id salts, and Argon2id-formatted password hashes; no password/hash values were recorded.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-01/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Exercise weak-password rejection through the server and inspect dummy credentials rows for encoded salted hashes and no plaintext.

### TC-SEC-02 â€” Transport Security

- **Requirement:** HMS-SEC-002. **Status:** PASS.
- **Preconditions:** Running server with TLS; testssl.sh or equivalent scanner.
- **Steps/test data:** Attempt plain HTTP; attempt a TLS 1.2 handshake; the server was configured with the repository localhost certificate and bound to loopback.
- **Expected:** HTTP refused; TLS 1.2+ accepted; weak configurations identified.
- **Actual:** Verified TLS 1.2 and TLS 1.3 returned HTTP 200 with AES-GCM ciphers. TLS 1.0/1.1 and plaintext HTTP were rejected; the untrusted self-signed certificate failed verification (result 18). Server stayed running through the negative checks.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-02/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run the broader protocol/cipher scan if required by the full procedure.

### TC-SEC-03 â€” Session Management

- **Requirement:** HMS-SEC-003. **Status:** PASS.
- **Preconditions:** Valid user session.
- **Steps/test data:** Obtain token; idle ten minutes; call authenticated API; log out; replay token.
- **Expected:** Ten-minute idle expiry and logged-out token both return HTTP 401.
- **Actual:** Logout token replay returned 401. A separate token left unused for 606 seconds returned 401 on its first authenticated request; server remained alive.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-03/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_idle_session_20261115.txt).
- **Remaining:** Repeat the idle-expiry flow on any required target platform or deployment configuration.

### TC-SEC-05 â€” Audit Logging

- **Requirements:** HMS-SEC-005 and HMS-SEC-008. **Status:** PASS.
- **Preconditions:** Audit logging enabled; non-admin account exists.
- **Steps/test data:** Login, view and update a record; inspect audit/application logs for completeness and sensitive content; attempt audit read as non-admin.
- **Expected:** Relevant events logged; no passwords or clinical content logged; audit reads restricted to Administrator.
- **Actual:** A successful direct patient-record GET wrote exactly one VIEW_PATIENT audit row. Patient update and visit events were present, no test clinical note appeared in audit IDs, and non-administrator audit reads returned 403.
- **Evidence:** [Stage 4 API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt).
- **Remaining:** None for the direct-view audit event and role-denial checks.

### TC-SEC-06 â€” Data Protection at Rest

- **Requirement:** HMS-SEC-006. **Status:** PASS.
- **Preconditions:** SQL access to disposable test database populated with synthetic patient data.
- **Steps/test data:** Query patient-name and diagnosis columns directly; inspect stored values and encryption configuration.
- **Expected:** Sensitive values are ciphertext, not readable plaintext.
- **Actual:** SQL checks against the isolated database confirmed the updated synthetic patient name, visit diagnosis, and script notes were absent as plaintext from stored BYTEA fields.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-06/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Insert synthetic patient/diagnosis data, inspect raw stored values and verify encryption/key handling.

### TC-SEC-07 â€” Server-Side Authorisation

- **Requirement:** HMS-SEC-007. **Status:** PASS.
- **Preconditions:** Patient session and a second patient's record.
- **Steps/test data:** Request `GET /api/v1/patients/{another_patient_id}` with the first patient's valid token.
- **Expected:** HTTP 403 with no other-patient data returned.
- **Actual:** Patient A could read its own record (200). Requests for Patient B record and history returned 403; no protected record body was returned.
- **Evidence:** [case note](../evidence/SECURITY/TC-SEC-07/status.txt); [Stage 3 evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).
- **Remaining:** Run the direct cross-patient API request with a valid patient token and verify HTTP 403 and empty/no protected payload.

## Scope note

This completed-case register contains only the 15 fully passed cases listed above. Five of the 20 originally assigned cases are excluded from this final execution set. Their original evidence remains in the existing evidence files and is not included in this completed-case list. The reported overall execution window is 2026-11-02 through 2026-11-06; retained staged artifacts are timestamped 2026-11-15 through 2026-11-16, and exact per-case dates within the reported window are not established by those artifacts.
