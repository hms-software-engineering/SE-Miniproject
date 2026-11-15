# HMS Defect Report

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

## Remaining confirmed application defects

These observed route or behavior failures are separate from the repaired TLS issue:

- `GET /api/v1/doctors/me/schedule` returns 404; the alternate schedule route returns 501.
- Patient own-history and own-bills routes return 404.
- Bill creation, payment, receipt, revenue report, and administrator audit-filter requests return 501.
- Direct patient-record reads do not emit the expected `VIEW_PATIENT` audit event. Non-administrator audit reads are denied with 403.

See the [execution log](test_execution_log.md) and [case details](test_case_execution_details.md) for case outcomes. This run recorded these behaviors; it did not implement unrelated application features.
