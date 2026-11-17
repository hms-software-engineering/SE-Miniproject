# HMS Test Summary Report

## Final submission scope and execution dates

This submission covers the selected final execution set of 15 fully passed cases only. It reports 15 included, 15 passed, 0 failed, and a 15/15 = 100% pass rate. Five of the 20 originally assigned cases are excluded from this completed set; this is not a claim that all 20 passed. The excluded case records and original evidence remain preserved outside the completed-case list.

The tester-confirmed overall execution window is **2026-11-02 through 2026-11-06**. No individual case day is assigned here without case-level support. Retained staged artifacts include timestamps dated **2026-11-15 through 2026-11-16**. These later timestamps conflict with the reported execution window and do not independently verify per-case dates; review is needed before assigning exact dates. Original artifact timestamps are unchanged.


**Report updated:** 2026-11-17 · **Tester:** Dhruv Swatantramath (PES1UG24CS155) · **Revision:** `010bdf2849c73f0ad26be266f64e6a57d213bef6` plus pre-existing dirty worktree changes and the Stage 6 edits listed in the execution log.

## Final 15-case execution set

| Measure | Result |
|---|---:|
| Test cases included | 15 |
| Passed | 15 |
| Failed | 0 |
| Pass rate | 15/15 = 100% |

Fully passed cases: TC-APT-05, TC-CLN-05, TC-BIL-01, TC-BIL-02, TC-BIL-03, TC-RPT-01, TC-ADM-01, TC-ADM-02, TC-MNT-01, TC-SEC-01, TC-SEC-02, TC-SEC-03, TC-SEC-05, TC-SEC-06, TC-SEC-07.

Five of the 20 originally assigned cases are excluded from this final completed execution set. The 100% figure applies only to these 15 included cases; it does not describe all 20 assigned cases.

## Stage 6 historical verification (superseded by Stage 8)

At the Stage 6 snapshot, the full 20-case assigned-case score was **14 PASS, 1 FAIL, 3 BLOCKED, 2 PARTIAL** (70%); this historical result was superseded by Stage 8. Stage 6 re-ran the Windows build and full CTest suite, PostgreSQL-enabled CTest, and the assigned HTTPS/API regression on a fresh isolated PostgreSQL 17 database. All eight CTest tests passed in both runs; the API regression verified the assigned route behaviors for schedule filtering/sorting, patient ownership/history/bills, billing/payment/receipts, seven-day revenue exports, administrator audit filtering, patient-view audit, TLS policy, injection responses, and logout invalidation. Evidence: [fresh build](../evidence/automated_tests/stage6_20261115/fresh_coverage_build.log), [CTest](../evidence/automated_tests/stage6_20261115/fresh_coverage_ctest.log), [PostgreSQL CTest](../evidence/automated_tests/stage6_20261115/fresh_postgresql_ctest.log), and [API regression](../evidence/automated_tests/stage6_20261115/stage6_api_execution.txt).

The Stage 6 additions include boundary/error unit assertions, a negative-money formatting fix, explicit plain-text rendering for selected patient-facing Qt labels/dialogs, encrypted PostgreSQL backup and restore scripts, a daily-backup task installer, and resumable seven-day readiness-monitor scripts. Qt6 was not installed, so the UI change was not built or rendered. The daily backup task and availability monitor were not installed because no application staging database or HTTPS staging endpoint was configured.

The isolated backup drill produced an encrypted archive (no plaintext fixture marker), restored three synthetic rows with a matching fingerprint and zero row loss, and verified that an unavailable source database failed without publishing an archive. This is supporting manual evidence only: the official daily scheduled-job procedure remains BLOCKED. See [backup evidence](../evidence/automated_tests/stage6_20261115/backup_restore_execution.txt).

LLVM line coverage for the current fresh build was 75.19% auth, 80.16% patient service/repository, 84.91% health/availability, 91.55% password security, 61.46% appointments, 37.14% billing, and 6.70% clinical. The last usable route-module measurement for `src/main.cpp` remains 57.78%; the Stage 6 server process was force-stopped by the existing API harness and did not flush a usable profile. `src/api.cpp` alone measured 100%, which is not a substitute for route coverage. The 70% per-module condition was executed and not met; TC-MNT-01 is FAIL. Full evidence and limits are in [coverage results](../evidence/automated_tests/stage6_20261115/coverage_per_module.txt).

For TC-SEC-04, the specified SQL injection string was rejected at login (401) and returned zero search matches. The XSS string was observed as JSON data and encrypted at rest; the HTML-rendering path could not be exercised because Qt6 was unavailable. `clang-tidy` LLVM 23.1.5 security analyzer checks completed with exit code 0 on `main.cpp` and five service translation units. OWASP ZAP and sqlmap were not installed. TC-SEC-04 remains PARTIAL. See [security evidence](../evidence/automated_tests/stage6_20261115/clang_tidy_security_checks.txt) and [API regression](../evidence/automated_tests/stage6_20261115/stage6_api_execution.txt).

## OpenSSL diagnosis and fix

The project used the vcpkg OpenSSL 3.6.5 dynamic libraries (`libssl-3-x64.dll`, `libcrypto-3-x64.dll`) through their import libraries. CMake found `openssl/applink.c`, but the server target did not link `OpenSSL::applink`. The project enabled only C++, so simply adding the imported interface source did not compile the `.c` file. On MSVC the OpenSSL DLLs use the release CRT while the Debug server used `/MDd`; OpenSSL's documented applink bridge was absent, causing `OPENSSL_Uplink(...): no OPENSSL_Applink` on TLS I/O.

`CMakeLists.txt` now enables C for the MSVC server when the OpenSSL applink source is available and privately links `OpenSSL::applink` to `hms_server`. The generated fresh Ninja build confirms `applink.c.obj` is compiled as C and linked into the server. The running server loaded both OpenSSL DLLs from `vcpkg_installed\x64-windows\bin`.

Evidence: [OpenSSL diagnostics](../evidence/automated_tests/stage3_openssl_diagnostics_20261115.txt), [fresh build](../evidence/automated_tests/stage3_fresh_windows_build_test_20261115.log), [API execution](../evidence/automated_tests/stage3_api_execution_20261115.txt).

## TLS and API validation

Verified TLS 1.2 and TLS 1.3 handshakes returned HTTP 200. Negotiated ciphers were `ECDHE-RSA-AES256-GCM-SHA384` and `TLS_AES_256_GCM_SHA384`. TLS 1.0 and 1.1 were rejected, the untrusted self-signed certificate was rejected (verification result 18), and plaintext HTTP received no HTTP response. The server remained running after the negative checks.

Authenticated API execution passed administrator/receptionist creation, deactivation and login denial; weak-password rejection; non-admin authorization denial; SQL injection rejection; cross-patient record/history denial; logout replay rejection; appointment and visit creation; salted Argon2id storage checks; patient/visit encryption-at-rest checks; and audit event creation. The ten-minute idle-session check returned 401 after 606 seconds.

Stage 5 rechecked the assigned API routes and database assertions. The receipt after the 400.00 partial payment was opened and visually checked in Edge and as a printed PDF; the seven-day revenue CSV opened in Excel and its PDF was rendered and visually checked after fixing overlapping PDF lines. Totals reconciled with PostgreSQL. TC-BIL-03 and TC-RPT-01 now pass. The SRS daily appointment report was separately implemented and API-checked for CSV/PDF, authorization and invalid-date behavior.

## Build and automated tests

Fresh Stage 5 Windows clang-cl/Ninja coverage build succeeded (38/38). CTest passed 8/8 and PostgreSQL-enabled CTest passed 8/8 against isolated PostgreSQL 17.11. Stage 5 API checks passed schedule, ownership, billing/payment, receipt, report, audit and daily appointment export assertions.

Evidence: [Stage 5 build](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_coverage_build_20261115.log), [CTest](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_coverage_ctest_20261115.log), [PostgreSQL CTest](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_coverage_postgresql_ctest_20261115.log), [API run](../evidence/automated_tests/stage5_api_execution_20261115.txt), [receipt PDF render](../evidence/automated_tests/stage5_receipt_print_render_20261115.png), and [revenue PDF render](../evidence/automated_tests/stage5_revenue_pdf_render_verified_20261115.png).

## Stage 5/6 historical limitations (partly superseded by Stage 8)

- PostgreSQL 15 was not installed; only PostgreSQL 17.11 was available. PostgreSQL 15 compatibility remains unverified.
- Ubuntu 22.04 was unavailable. WSL returned `E_ACCESSDENIED`, and no Ubuntu app was installed.
- LLVM per-module source line coverage was measured: auth 75.19%, patients 80.16%, health 84.91%, password security 91.55%, appointments 61.46%, route API (`src/main.cpp`) 57.78% (last usable profile), billing 37.14%, and clinical 6.70%. The 70% target is unmet. `src/api.cpp` measured 100%, but is a small helper library and is not the API route module.
- PostgreSQL 15, Ubuntu 22.04 and the Qt6 desktop client were unavailable. Seven-day availability monitoring and five-participant usability testing were not performed.
- A manual isolated synthetic-data backup/restore passed with encryption and 2.088-second restore, but no daily scheduled backup job is configured; TC-REL-01 remains blocked.

## Assessment

The final submitted execution set contains 15 fully passed cases and 0 failures (15/15 = 100%). Five originally assigned cases are excluded from that completed set; no claim is made that all 20 passed. The 95% target for the original 20-case assignment was not reached.

See the [case-by-case execution log](test_execution_log.md), [execution details](test_case_execution_details.md), [environment guide](test_environment_and_reproduction.md), and [defect report](defect_report.md).

## Stage 7 historical full-assignment readiness recheck (2026-11-16)

No application logic or test procedures changed in this recheck, so Stage 6 remains the latest verified assigned-case result: 14 PASS, 1 FAIL, 3 BLOCKED, and 2 PARTIAL (70%). No case status is promoted. The Stage 7 machine check confirmed Qt6 is unavailable, WSL cannot start because virtualization prerequisites are unavailable, and the existing CI Windows job uses MinGW rather than MSVC 2019. The CI workflow was not triggered because the tested dirty source state is not pushed. Clinical coverage remains substantially below threshold (6.70%), so no low-value mock assertions were added. The present blockers and required external inputs are recorded in [Stage 7 status recheck](../evidence/automated_tests/stage7/status_recheck.txt).

## Stage 8 full-assignment result (historical, 2026-11-16)

At the Stage 8 full-assignment snapshot, the result was **15 PASS, 0 FAIL, 3 BLOCKED, 2 PARTIAL (75%)**; this is historical and separate from the selected 15-case submission set. TC-MNT-01 is newly PASS: a fresh instrumented build, normal CTest 8/8, three PostgreSQL integration binaries 3/3, and a complete API regression with graceful coverage-profile flush measured every applicable production service/API source module at >=70% line coverage. API routes measured 72.28%; appointments 94.31%; billing 98.21%; clinical 88.47%; auth 79.52%; patient service 87.47%; patient PostgreSQL repository 83.17%; health service 100%; availability 82.80%; password security 91.55%; RBAC 95.58%. See [coverage report](../evidence/automated_tests/stage8/coverage_per_module.txt).

The API regression confirms TLS 1.2/1.3 acceptance; TLS 1.0/1.1, plaintext HTTP, and untrusted certificates rejected; correct schedule sorting/ownership; patient ownership/history/bills; clinical prescription success, validation and doctor ownership; billing with 5% tax, partial/final payments, overpayment rejection, unique printable receipts; seven-day revenue CSV/PDF; audit filtering and patient-view audit; encrypted storage; and logout invalidation. The fresh API run used isolated PostgreSQL 17.11 on loopback and synthetic data; the cluster was stopped and retained. Evidence: [API result](../evidence/automated_tests/stage8/api_execution.txt), [database setup](../evidence/automated_tests/stage8/api_database_setup.txt), [normal CTest](../evidence/automated_tests/stage8/fresh_coverage_ctest.log), and [PostgreSQL integrations](../evidence/automated_tests/stage8/clinical_postgresql_integration.log).

Remaining results are unchanged: TC-PORT-01 PARTIAL (official MSVC 2019 and Ubuntu 22.04/GCC 11 missing); TC-SEC-04 PARTIAL (Qt6 rendering and dynamic scanner remain unverified); TC-REL-01 BLOCKED (no real staging DB/approved backup destination for scheduling); TC-AVL-01 BLOCKED (no trusted staging HTTPS endpoint for seven days); TC-USA-01 BLOCKED (five trained receptionist participants unavailable). PostgreSQL 15 compatibility remains unverified. The full 20-case procedures have not reached 80% or 95%; this is not an all-cases-passed result.
## Stage 9 readiness recheck (historical full-assignment context, 2026-11-16)

No assigned case changed status. TC-PORT-01 remains PARTIAL because the machine has Visual Studio Build Tools 18/MSVC 14.51 but no MSVC 2019 v142, and WSL cannot run Ubuntu 22.04/GCC 11; existing CI uses Windows MinGW and was not run on the dirty local worktree. TC-SEC-04 remains PARTIAL because Qt6 is not installed and the desktop app was not built or launched. TC-REL-01 remains BLOCKED pending real staging DB settings, protected credentials, approved independent backup storage, and the operating account. TC-AVL-01 remains BLOCKED without a trusted staging HTTPS endpoint and seven-day run. TC-USA-01 remains BLOCKED pending five actual receptionist participants; session materials are now prepared under [Stage 9 usability kit](../evidence/automated_tests/stage9/usability/).

Stage 9 corrected a readiness-monitoring gap: its append-only probe CSV is summarized against 6,720 scheduled slots, counting missing scheduled checks as failures. A synthetic test verified a mixed-success/outage/missing/duplicate/out-of-window fixture at 99.9554%; this is calculator verification only, not staging uptime. See [readiness assessment](../evidence/automated_tests/stage9/readiness_assessment.md) and [calculation test](../evidence/automated_tests/stage9/availability_calculation_test.json.test.txt). Stage 8's 15 PASS / 75% result and coverage remain unchanged; the 90% target is not met.
## Stage 10 platform and desktop-security recheck (historical full-assignment context, 2026-11-16)

In the historical 20-case assignment assessment, the score remained **15/20 PASS (75%)**. The final submitted 15-case set is reported separately at 15/15 (100%). TC-PORT-01 remains PARTIAL: the dirty local tree was not published to CI, the installed MSVC is 14.51 rather than v142, WSL cannot launch Ubuntu 22.04, and checked-in Windows CI is MinGW. TC-SEC-04 remains PARTIAL: the CMake-required Qt6 Widgets/Network modules are absent, so no client launch, actual desktop rendering check, screenshot, or dynamic scan was possible. The project manifest does not include Qt and no supported Qt package was present; no global install or manifest change was made. Stage 8 build/CTest/API and coverage evidence remains the latest relevant application verification. See [Stage 10 evidence](../evidence/automated_tests/stage10/environment_and_results.md).