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

Stage 5 opened and checked the partial-payment receipt in Edge and printed PDF, and opened the revenue CSV in Excel and rendered the revenue PDF. Both assigned cases now pass. A visual check exposed overlapping revenue PDF lines; the PDF text stream now sets 14-point leading, with regression assertions in `tests/billing_service_test.cpp`. The SRS daily appointment report was added and API-checked for CSV/PDF, access control, date validation and appointment content. Coverage measured below 70% for appointments (60.44%), API (57.78%), billing (31.18%) and clinical (6.70%); TC-MNT-01 is FAIL because coverage was measured below its acceptance threshold. The Stage 5 coverage record was superseded by the later Stage 8 coverage measurement.

## Stage 8 coverage verification for the included maintainability case

The fresh Stage 8 Windows Clang-cl 23.1.5/Ninja build completed 38/38 steps, CTest passed 8/8, and PostgreSQL integration targets passed 3/3. Source-only line coverage met the 70% threshold for each measured applicable module: API routes 72.28%, appointments 94.31%, billing 98.21%, clinical 88.47%, auth 79.52%, patient service 87.47%, patient repository 83.17%, health 100%, availability 82.80%, password security 91.55%, and RBAC 95.58%. This supports PASS for TC-MNT-01 in the selected set. See [Stage 8 API evidence](../evidence/automated_tests/stage8/api_execution.txt) and [coverage report](../evidence/automated_tests/stage8/coverage_per_module.txt).
