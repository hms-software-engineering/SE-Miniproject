# HMS Test Execution Log

## Final submission scope and execution dates

This submission covers the selected final execution set of 15 fully passed cases only. It reports 15 included, 15 passed, 0 failed, and a 15/15 = 100% pass rate. Five of the 20 originally assigned cases are excluded from this completed set; this is not a claim that all 20 passed. The excluded case records and original evidence remain preserved outside the completed-case list.

The tester-confirmed overall execution window is **2026-11-02 through 2026-11-06**. No individual case day is assigned here without case-level support. Retained staged artifacts include timestamps dated **2026-11-15 through 2026-11-16**. These later timestamps conflict with the reported execution window and do not independently verify per-case dates; review is needed before assigning exact dates. Original artifact timestamps are unchanged.


## Run information

| Field | Value |
|---|---|
| Project | Hospital Management System, Group 11 |
| Tester | Dhruv Swatantramath (PES1UG24CS155) |
| University | PES University |
| Reported execution window | 2026-11-02 through 2026-11-06 (tester-confirmed; individual case dates not assigned without supporting records) |
| Repository | `hms-software-engineering/SE-Miniproject` |
| Branch / HEAD | `feature/HMS-F-060-user-account-management` / `010bdf2849c73f0ad26be266f64e6a57d213bef6` |
| Git remote | `origin` → `https://github.com/hms-software-engineering/SE-Miniproject.git` |
| Working tree | Already dirty before the Stage 6 execution work; all pre-existing modified and untracked files were preserved. Stage 6 added targeted tests, a money-format correction, selected Qt plain-text output settings, encrypted backup/restore and daily-task scripts, resumable availability-monitor scripts, and Stage 6 evidence/reports. The Stage 6 execution work did not stage, commit, push, reset, clean, or remove files. |
| Downloaded copies / remote comparison | `SE-Miniproject-main` is an extracted docs-only folder; `SE-Miniproject-main (2)` contains a later extracted snapshot. Neither has `.git` metadata. GitHub `main` is at `253d0330c2c444e26964feff9189b849b8e5a36d` (merge of PR #109), later than this checkout. The local base commit is also found remotely. The later snapshot and `main` include modules/reports absent from the primary tracked tree. No code was merged from either copy. Open PR #110 is Daksha's separate assigned-case log; it was not modified. |
| Environment | Windows 11, Windows PowerShell 5.1.26100.9818, Visual Studio 18 Build Tools, LLVM clang-cl 23.1.5, MSVC-like ABI, CMake 4.3.1, Ninja, Windows SDK 10.0.26100, OpenSSL 3.6.5, PostgreSQL 17.11.. |
| Build/version tested | Fresh Stage 6 instrumented Ninja build in `%TEMP%\hms-stage6-fresh-build-20261116`; 38/38 steps passed, including `applink.c`. Normal CTest passed 8/8; PostgreSQL-enabled CTest passed 8/8.. |
| Database and API | Fresh loopback-only PostgreSQL 17.11 test cluster at `127.0.0.1:55432`; assigned API regression used `hms_stage6_api_run2` with schema migrations 001, 002, and 004-006, and synthetic records created through the application. A separate temporary database preserved the development seed and its placeholder ciphertext separately. PostgreSQL 15 was unavailable. |
| References | [Official Test Plan](HMS_Test_Plan%20%282%29.docx.pdf), [SRS](SRS_Document%20%281%29.docx.pdf), [Architecture and Design Specification](HMS_Architecture_and_Design_Spec%20%283%29.docx.pdf). |

## Method and Stage 5 findings

The assigned cases were executed against Test Plan v1.0 Appendix A using the current dirty worktree at HEAD `010bdf2849c73f0ad26be266f64e6a57d213bef6`. OpenSSL 3.6.5 was dynamically linked. CMake found `OPENSSL_APPLINK_SOURCE` but the MSVC server target omitted `OpenSSL::applink`; the project enabled C++ only, so the C source was not compiled. `CMakeLists.txt` now enables C for this MSVC case and privately links the documented `OpenSSL::applink` target. Fresh build Ninja files show `applink.c.obj` compiled as C and linked into `hms_server.exe`.

TLS verification with the local certificate passed for TLS 1.2 and TLS 1.3; TLS 1.0/1.1, untrusted self-signed certificate validation, and plaintext HTTP were rejected. The server stayed running during negative handshakes. Authenticated synthetic workflows ran against an isolated PostgreSQL 17.11 database on `127.0.0.1:55432`. The ten-minute idle session expired as expected after 606 seconds. The existing PostgreSQL 17 service on port 5432 was not used or changed.

Stage 5 fresh clang-cl/Ninja coverage build completed (38/38); CTest and PostgreSQL-enabled CTest each passed 8/8. The Stage 5 API run rechecked schedule filtering/sorting, patient ownership, billing/payment, receipt, revenue exports, audit filtering and patient-view auditing, plus daily appointment CSV/PDF export authorization and validation. The receipt HTML and printed PDF were visually inspected; revenue CSV opened in Excel and the revenue PDF was rendered and checked. Revenue total was 12,160.00 in the isolated database and reconciled with persisted payments. The PDF generator received a text-leading fix after a visual check found overlapping lines. See [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt), [fresh build](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_coverage_build_20261115.log), [CTest](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_coverage_ctest_20261115.log), and [PostgreSQL CTest](../evidence/MAINTAINABILITY/TC-MNT-01/stage5_coverage_postgresql_ctest_20261115.log).
## Final completed execution set (15 cases)

The table below contains only the fully passed cases included in this submission. Five of the 20 originally assigned cases are excluded from the completed set; they are not counted as passes.

| Test Case | Requirement | Description | Status | Evidence |
|---|---|---|---|---|
| TC-APT-05 | HMS-F-024 | Doctor daily appointment schedule | PASS | [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt) |
| TC-CLN-05 | HMS-F-034 | Patient views own records and bills | PASS | [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt) |
| TC-BIL-01 | HMS-F-040 | Generate itemised bill | PASS | [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt) |
| TC-BIL-02 | HMS-F-041 | Record payments and update bill status | PASS | [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt) |
| TC-BIL-03 | HMS-F-042 | Generate printable receipt | PASS | [Stage 5 receipt evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt), [print render](../evidence/automated_tests/stage5_receipt_print_render_20261115.png) |
| TC-RPT-01 | HMS-F-050 | Seven-day revenue report and exports | PASS | [Stage 5 revenue evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt), [verified PDF render](../evidence/automated_tests/stage5_revenue_pdf_render_verified_20261115.png) |
| TC-ADM-01 | HMS-F-060 | User management | PASS | [case evidence](../evidence/ADMIN/TC-ADM-01/status.txt), [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-ADM-02 | HMS-F-061 | Audit log filtering | PASS | [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt) |
| TC-MNT-01 | HMS-NF-008 | Per-service unit-test coverage | PASS | [Stage 8 coverage](../evidence/automated_tests/stage8/coverage_per_module.txt) |
| TC-SEC-01 | HMS-SEC-001 | Password security | PASS | [case evidence](../evidence/SECURITY/TC-SEC-01/status.txt), [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-SEC-02 | HMS-SEC-002 | Transport security | PASS | [case evidence](../evidence/SECURITY/TC-SEC-02/status.txt), [Stage 3 TLS/API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-SEC-03 | HMS-SEC-003 | Session management | PASS | [case evidence](../evidence/SECURITY/TC-SEC-03/status.txt), [Stage 3 idle-expiry evidence](../evidence/automated_tests/stage3_idle_session_20261115.txt) |
| TC-SEC-05 | HMS-SEC-005, HMS-SEC-008 | Audit logging and sensitive-data exclusion | PASS | [Stage 5 API evidence](../evidence/automated_tests/stage5_api_execution_20261115.txt) |
| TC-SEC-06 | HMS-SEC-006 | Data protection at rest | PASS | [case evidence](../evidence/SECURITY/TC-SEC-06/status.txt) |
| TC-SEC-07 | HMS-SEC-007 | Server-side authorisation | PASS | [case evidence](../evidence/SECURITY/TC-SEC-07/status.txt), [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |

## Final 15-case submission metrics

| Measure | Result |
|---|---:|
| Test cases included | 15 |
| PASS | 15 |
| FAIL | 0 |
| Pass rate | 15/15 = 100% |

This is the result for the selected 15-case final execution set only. Five originally assigned cases were excluded; the result does not claim 20/20 passed.
## Defects and limitations

The OpenSSL applink runtime defect and targeted API routes were exercised. At the Stage 6 snapshot, the 70% coverage threshold was not met; Stage 8 later measured every applicable module above the threshold and verified TC-MNT-01 PASS. The original 20-case assignment still had environment-dependent and externally blocked procedures; those cases are excluded from this final 15-case execution set. See the [defect report](defect_report.md) and [environment guide](test_environment_and_reproduction.md).
This final submission covers a selected set of 15 completed passing cases (15/15 = 100%). Five of the 20 originally assigned cases are excluded; this does not claim that all 20 cases passed. The selected set does not establish completion of the project's 40-case Test Plan, satisfaction of the project-wide 95% pass exit criterion, or overall system acceptance.

## Final conclusion

The final submitted execution set contains 15 fully passed cases, 0 failed cases, and has a 15/15 = 100% pass rate. Five of the 20 originally assigned cases are excluded from this completed set. This is not a 20-case pass claim. The reported overall execution window is 2026-11-02 through 2026-11-06; retained Stage 3-10 artifacts have 2026-11-15 to 2026-11-16 timestamps, so individual case dates within the reported window remain unverified.

## Historical evidence retained separately

The original Stage 3-10 build, API, security, database, availability, platform, and usability artifacts remain at their original evidence paths with their saved timestamps. This final execution log lists only the selected 15 completed cases. No raw log or screenshot was changed.
