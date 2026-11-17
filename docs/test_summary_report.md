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

## Historical Stage 6 evidence

Earlier Stage 6 build, API, and security evidence remains at its original evidence paths and in Git history. It is not the result for the selected 15-case execution set.

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

## Evidence limitations

PostgreSQL 15 compatibility remains unverified. Stage 8 evidence was obtained on local revision `010bdf2849c73f0ad26be266f64e6a57d213bef6` with uncommitted changes and was not rerun on former PR head `c7f2673d4b3a1cf8e3ca080dbd6ab4f0338daa70`. The timestamped supporting artifacts were saved 2026-11-15 through 2026-11-16; the reported execution window remains 2026-11-02 through 2026-11-06, and exact per-case dates are not inferred from those later artifact timestamps.

## Assessment

The final submitted execution set contains 15 fully passed cases and 0 failures (15/15 = 100%). Five originally assigned cases are excluded from that completed set; no claim is made that all 20 passed. The 95% target for the original 20-case assignment was not reached.

See the [case-by-case execution log](test_execution_log.md), [execution details](test_case_execution_details.md), [environment guide](test_environment_and_reproduction.md), and [defect report](defect_report.md).



## Stage 8 validation evidence for the included coverage case

The Stage 8 fresh Windows Clang-cl 23.1.5/Ninja build completed 38/38 steps, CTest passed 8/8, and PostgreSQL integration targets passed 3/3. The source-only line coverage measurement met the 70% threshold for each measured applicable module: API routes 72.28%, appointments 94.31%, billing 98.21%, clinical 88.47%, auth 79.52%, patient service 87.47%, patient PostgreSQL repository 83.17%, health 100%, availability 82.80%, password security 91.55%, and RBAC 95.58%. This supports TC-MNT-01 in the selected set.

The Stage 8 API regression supplied supporting verification evidence for the included schedule, patient ownership/history, billing/payment/receipt, revenue export, audit, transport security, encryption, and session checks. The isolated PostgreSQL 17.11 cluster was stopped after the run. Evidence: [coverage report](../evidence/automated_tests/stage8/coverage_per_module.txt), [build](../evidence/automated_tests/stage8/fresh_coverage_build.log), [CTest](../evidence/automated_tests/stage8/fresh_coverage_ctest.log), [PostgreSQL integrations](../evidence/automated_tests/stage8/clinical_postgresql_integration.log), and [API run](../evidence/automated_tests/stage8/api_execution.txt).
