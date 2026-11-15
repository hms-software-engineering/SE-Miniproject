# HMS Test Summary Report

**Date:** 2026-11-15 · **Tester:** Dhruv Swatantramath (PES1UG24CS155) · **Revision:** `010bdf2849c73f0ad26be266f64e6a57d213bef6` plus pre-existing dirty worktree changes.

## Assigned 20-case results

| Measure | Result |
|---|---:|
| Assigned cases | 20 |
| PASS | 6 |
| FAIL | 6 |
| BLOCKED | 6 |
| PARTIAL | 2 |
| Total | 20 |
| Cases with direct assigned-case evidence | 14/20 |
| Full-pass rate | 6/20 = 30% |

Case-by-case statuses and original procedure descriptions are in the [execution log](test_execution_log.md) and [execution details](test_case_execution_details.md).

## OpenSSL diagnosis and fix

The project used the vcpkg OpenSSL 3.6.5 dynamic libraries (`libssl-3-x64.dll`, `libcrypto-3-x64.dll`) through their import libraries. CMake found `openssl/applink.c`, but the server target did not link `OpenSSL::applink`. The project enabled only C++, so simply adding the imported interface source did not compile the `.c` file. On MSVC the OpenSSL DLLs use the release CRT while the Debug server used `/MDd`; OpenSSL's documented applink bridge was absent, causing `OPENSSL_Uplink(...): no OPENSSL_Applink` on TLS I/O.

`CMakeLists.txt` now enables C for the MSVC server when the OpenSSL applink source is available and privately links `OpenSSL::applink` to `hms_server`. The generated fresh Ninja build confirms `applink.c.obj` is compiled as C and linked into the server. The running server loaded both OpenSSL DLLs from `vcpkg_installed\x64-windows\bin`.

Evidence: [OpenSSL diagnostics](../evidence/automated_tests/stage3_openssl_diagnostics_20261115.txt), [fresh build](../evidence/automated_tests/stage3_fresh_windows_build_test_20261115.log), [API execution](../evidence/automated_tests/stage3_api_execution_20261115.txt).

## TLS and API validation

Verified TLS 1.2 and TLS 1.3 handshakes returned HTTP 200. Negotiated ciphers were `ECDHE-RSA-AES256-GCM-SHA384` and `TLS_AES_256_GCM_SHA384`. TLS 1.0 and 1.1 were rejected, the untrusted self-signed certificate was rejected (verification result 18), and plaintext HTTP received no HTTP response. The server remained running after the negative checks.

Authenticated API execution passed administrator/receptionist creation, deactivation and login denial; weak-password rejection; non-admin authorization denial; SQL injection rejection; cross-patient record/history denial; logout replay rejection; appointment and visit creation; salted Argon2id storage checks; patient/visit encryption-at-rest checks; and audit event creation. The ten-minute idle-session check returned 401 after 606 seconds.

The server returned 404 for patient own history/bills and the assigned doctor schedule route, and 501 for audit filtering, the alternate doctor schedule route, billing, payments, receipts and revenue reports. These are recorded as case failures or blocked prerequisites, not passes.

## Build and automated tests

Fresh Windows build succeeded in `%TEMP%\hms-stage3-msvc-ninja-20261115` using MSVC 19.51.36260, CMake 4.3.1-msvc1, and Ninja 1.13.2. CTest passed 7/7 with the vcpkg DLL directory on `PATH`. PostgreSQL-enabled CTest also passed 7/7 against the isolated PG17.11 database.

Evidence: [Stage 3 CTest](../evidence/automated_tests/stage3_ctest_20261115.log), [PostgreSQL integration CTest](../evidence/automated_tests/stage3_postgresql_ctest_20261115.log), [idle expiry](../evidence/automated_tests/stage3_idle_session_20261115.txt).

## Remaining limitations

- PostgreSQL 15 was not installed; only PostgreSQL 17.11 was available. PostgreSQL 15 compatibility remains unverified.
- Ubuntu 22.04 was unavailable. WSL returned `E_ACCESSDENIED`, and no Ubuntu app was installed.
- No gcov, lcov, OpenCppCoverage, or module-specific coverage report was available; the per-service 70% target remains unverified.
- Seven-day availability monitoring, isolated backup/restore, and five-participant usability testing were not performed.
- The API surface still lacks audit filtering, doctor schedule, patient history/bills, billing, payment, receipt, and revenue-report implementations observed in this worktree.

## Assessment

The OpenSSL runtime defect is fixed, and the local TLS/API is reachable with verified certificate checks. **Six cases passed, six failed, six remain blocked, and two are partial.** This is not submission-ready as completed assignment testing.

See the [case-by-case execution log](test_execution_log.md), [execution details](test_case_execution_details.md), [environment guide](test_environment_and_reproduction.md), and [defect report](defect_report.md).
