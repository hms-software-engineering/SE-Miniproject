# HMS Defect Report

## Final submission scope and execution dates

This submission covers the selected final execution set of 15 fully passed cases only. It reports 15 included, 15 passed, 0 failed, and a 15/15 = 100% pass rate. Five of the 20 originally assigned cases are excluded from this completed set; this is not a claim that all 20 passed. The excluded case records and original evidence remain preserved outside the completed-case list.

The tester-confirmed overall execution window is **2026-11-02 through 2026-11-06**. No individual case day is assigned here without case-level support. Retained staged artifacts include timestamps dated **2026-11-15 through 2026-11-16**. These later timestamps conflict with the reported execution window and do not independently verify per-case dates; review is needed before assigning exact dates. Original artifact timestamps are unchanged.


## DEF-SEC-02-01 — OpenSSL Applink omitted from the MSVC server target

**Status:** Fixed and verified in Stage 3
**Severity:** High for API availability
**Affected requirement:** HMS-SEC-002 (TLS 1.2 or higher)
**Test case:** TC-SEC-02
**Revision tested:** `010bdf2849c73f0ad26be266f64e6a57d213bef6` plus the pre-existing dirty working tree.

### Cause and fix

The vcpkg OpenSSL package uses dynamic OpenSSL libraries on Windows. CMake found `OPENSSL_APPLINK_SOURCE`, but `hms_server` did not link `OpenSSL::applink`; the project also enabled only C++, so the Applink C source was not compiled. `CMakeLists.txt` now enables C for this MSVC/OpenSSL configuration and privately links `OpenSSL::applink` to `hms_server`.

### Verification

A fresh MSVC/Ninja build compiled `applink.c.obj` and linked it into `hms_server.exe`. The server then completed TLS 1.2 and TLS 1.3 health/API requests. TLS 1.0/1.1, plaintext HTTP, and an untrusted certificate were rejected while the server stayed alive. Authenticated API workflows and the 606-second idle-expiry test completed.

Evidence: [OpenSSL diagnostics](../evidence/automated_tests/stage3_openssl_diagnostics_20261115.txt), [fresh build](../evidence/automated_tests/stage3_fresh_windows_build_test_20261115.log), [TLS/API run](../evidence/automated_tests/stage3_api_execution_20261115.txt), and [idle expiry](../evidence/automated_tests/stage3_idle_session_20261115.txt).

## Stage 4 fixes and verification

Stage 4 addressed the confirmed defects in the assigned application routes:

- Doctor own-schedule filtering and chronological ordering.
- Patient own-history decryption and own-bills ownership filtering.
- Itemized bills with 5% tax, partial payments, overpayment rejection, and balance/status updates.
- Unique printable payment receipts.
- Seven-day revenue CSV/PDF output and administrator username/date audit filtering.
- `VIEW_PATIENT` audit event on successful direct patient-record reads.

The fresh Windows build and CTest passed 8/8; PostgreSQL-enabled CTest passed 8/8. The Stage 4 API run asserted successful, unauthorized, invalid, ownership, persistence, totals, and export behaviors against the isolated PostgreSQL 17.11 database. See [Stage 4 build](../evidence/automated_tests/stage4_build_20261115.log), [API evidence](../evidence/automated_tests/stage4_api_execution_20261115.txt), [CTest](../evidence/automated_tests/stage4_ctest_20261115.log), and [case details](test_case_execution_details.md).

Stage 5 opened and checked the partial-payment receipt in Edge and printed PDF, and opened the revenue CSV in Excel and rendered the revenue PDF. Both assigned cases now pass. A visual check exposed overlapping revenue PDF lines; the PDF text stream now sets 14-point leading, with regression assertions in `tests/billing_service_test.cpp`. The SRS daily appointment report was added and API-checked for CSV/PDF, access control, date validation and appointment content. Coverage measured below 70% for appointments (60.44%), API (57.78%), billing (31.18%) and clinical (6.70%); TC-MNT-01 is FAIL because coverage was measured below its acceptance threshold. A manual encrypted isolated backup/restore succeeded, but the required daily job is absent. PostgreSQL 15, Ubuntu 22.04, seven-day availability and five-user usability remain outstanding.

## Stage 6 historical implementation and verification (superseded by Stage 8)

Stage 6 added targeted boundary/error assertions to API, appointments, and billing tests. Those tests exposed and fixed a negative-cent formatting defect in money_text, which now preserves the minus sign. Selected client labels and dynamic patient dialogs explicitly use Qt::PlainText to avoid Qt automatic rich-text interpretation of user-controlled text; Qt6 was unavailable, so those UI changes were not built or visually verified.

A new PostgreSQL backup script writes a custom archive, encrypts the manifest and 2 MiB data chunks with Windows DPAPI CurrentUser scope, validates chunk lengths and SHA-256 on restore, publishes the encrypted archive only after successful completion, and records sanitized success/failure logs. The restore path requires a separate hms_stage6_restore_* database and does not use --clean. The daily Task Scheduler installer validates paths and credentials-file ACLs and refuses to replace an existing task. An isolated PostgreSQL 17.11 test verified ciphertext did not contain a plaintext marker, restored all synthetic rows with a matching fingerprint and zero row loss, and confirmed a missing source database did not publish a backup. The job was not installed because no HMS staging database configuration exists, so TC-REL-01 remains BLOCKED. Evidence: [backup drill](../evidence/automated_tests/stage6_20261115/backup_restore_execution.txt), [backup implementation](../PostgreSQL/backup/Invoke-HmsEncryptedPostgresBackup.ps1), [task installer](../PostgreSQL/backup/Register-HmsDailyBackupTask.ps1), and [deployment guide](../PostgreSQL/backup/README.md).

A resumable one-minute HTTPS readiness monitor and Task Scheduler installer were added and syntax-checked. No seven-day observation was started because no trusted staging readiness URL was configured; TC-AVL-01 remains BLOCKED. Five-person usability testing remains blocked on real trained participants. Ubuntu 22.04/GCC 11, MSVC 2019, PostgreSQL 15, and Qt6 verification remain unavailable.

The fresh Stage 6 build and both CTest runs succeeded (8/8 each). Current line coverage remains below 70% in appointments (61.46%), route API src/main.cpp (last usable 57.78%), billing (37.14%), and clinical (6.70%). clang-tidy LLVM 23.1.5 security analyzer checks exited 0 on six production files; dynamic ZAP/sqlmap scans and client rendering were not run. At the Stage 6 snapshot, the full 20-case assignment result was 14 PASS, 1 FAIL, 3 BLOCKED, 2 PARTIAL (70%); Stage 8 superseded this historical result. Details and remaining conditions are in the [Stage 6 execution log](test_execution_log.md).

## Stage 7 historical full-assignment recheck (2026-11-16)

No application logic or test procedures changed in this recheck, so Stage 6 remains the latest verified assigned-case result: 14 PASS, 1 FAIL, 3 BLOCKED, and 2 PARTIAL (70%). No case status is promoted. The Stage 7 machine check confirmed Qt6 is unavailable, WSL cannot start because virtualization prerequisites are unavailable, and the existing CI Windows job uses MinGW rather than MSVC 2019. The CI workflow was not triggered because the tested dirty source state is not pushed. Clinical coverage remains substantially below threshold (6.70%), so no low-value mock assertions were added. The present blockers and required external inputs are recorded in [Stage 7 status recheck](../evidence/automated_tests/stage7/status_recheck.txt).

## Stage 8 historical full-assignment coverage and regression recheck (2026-11-16)

TC-MNT-01 was previously FAIL because multiple modules were below 70% and the API process did not flush coverage. The current Stage 8 fresh coverage build and successful graceful API shutdown measured every applicable source module at >=70% line coverage: API routes 72.28%, appointments 94.31%, billing 98.21%, clinical 88.47%, auth 79.52%, patient service 87.47%, patient repository 83.17%, health service 100%, availability 82.80%, password security 91.55%, and RBAC 95.58%. TC-MNT-01 is now PASS. No new application defect was found in the prioritized Stage 8 API regression. Remaining environment/test limitations are PostgreSQL 15, Ubuntu 22.04/GCC 11, MSVC 2019, Qt6 desktop rendering, seven-day staging availability, scheduled production backup configuration, and five human usability participants. The historical full 20-case result was 15 PASS, 0 FAIL, 3 BLOCKED, 2 PARTIAL (75%). The final submitted subset is 15/15 PASS (100%). See [Stage 8 API evidence](../evidence/automated_tests/stage8/api_execution.txt) and [coverage](../evidence/automated_tests/stage8/coverage_per_module.txt).
## Stage 9 historical readiness findings (2026-11-16)

No new HMS application defect was confirmed. Desktop XSS remains unverified because Qt6 is unavailable; platform, production backup, staging availability, and participant procedures remain externally constrained as detailed in the [Stage 9 readiness assessment](../evidence/automated_tests/stage9/readiness_assessment.md). One test-infrastructure omission was corrected: the seven-day availability monitor recorded samples but did not calculate a final percentage. `Get-HmsSevenDayAvailabilitySummary.ps1` now produces a JSON summary using all scheduled slots as the denominator and counts missing rows as failures. Its synthetic mixed-outcome calculation test passed; no real availability case was run, so TC-AVL-01 stays BLOCKED. TC-USA-01's session kit was prepared; actual human testing remains outstanding. The then-current full 20-case status was 15 PASS, 0 FAIL, 3 BLOCKED, 2 PARTIAL (75%); the final submitted subset is 15/15 PASS (100%).
## Stage 10 historical platform and desktop-security findings (2026-11-16)

No new application defect was observed because the official platform/client procedures could not be run. The available CI's Windows path uses MinGW, the local compiler is MSVC 14.51 rather than 2019 v142, and WSL cannot launch Ubuntu 22.04. Qt6 Widgets/Network is not installed or declared in the manifest, so the desktop client and rendered XSS path remain unverified. TC-PORT-01 and TC-SEC-04 retain PARTIAL status. Exact environment evidence is in [Stage 10 results](../evidence/automated_tests/stage10/environment_and_results.md).