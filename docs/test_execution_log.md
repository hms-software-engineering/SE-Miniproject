# HMS Test Execution Log

## Run information

| Field | Value |
|---|---|
| Project | Hospital Management System, Group 11 |
| Tester | Dhruv Swatantramath (PES1UG24CS155) |
| University | PES University |
| Execution date | 2026-11-15 (Asia/Calcutta) |
| Repository | `hms-software-engineering/SE-Miniproject` |
| Branch / HEAD | `feature/HMS-F-060-user-account-management` / `010bdf2849c73f0ad26be266f64e6a57d213bef6` |
| Git remote | `origin` → `https://github.com/hms-software-engineering/SE-Miniproject.git` |
| Working tree | Already dirty before this run. Existing source changes and untracked files were preserved; this run added the MSVC OpenSSL Applink CMake integration and updated requested reports/evidence. |
| Downloaded copies / remote comparison | `SE-Miniproject-main` is an extracted docs-only folder; `SE-Miniproject-main (2)` contains a later extracted snapshot. Neither has `.git` metadata. GitHub `main` is at `253d0330c2c444e26964feff9189b849b8e5a36d` (merge of PR #109), later than this checkout. The local base commit is also found remotely. The later snapshot and `main` include modules/reports absent from the primary tracked tree. No code was merged from either copy. Open PR #110 is Daksha's separate assigned-case log; it was not modified. |
| Environment | Windows 11 build 26300; PowerShell 5.1; Visual Studio 18 Build Tools; MSVC 19.51.36260; CMake 4.3.1-msvc1; Ninja 1.13.2; Windows SDK 10.0.26100; vcpkg OpenSSL 3.6.5. PostgreSQL 17.11 installed. |
| Build/version tested | Fresh Stage 3 Ninja/MSVC build in `%TEMP%\hms-stage3-msvc-ninja-20261115`; 33 steps passed including applink. CTest passed 7/7; PostgreSQL integration CTest passed 7/7. Source includes pre-existing dirty changes; Qt6 client skipped. |
| Database and API | Isolated PostgreSQL 17.11 cluster on `127.0.0.1:55432`; migrations 001-006 applied; Stage 3 API workflows and 606-second idle-expiry check completed. PostgreSQL 15 was unavailable. |
| References | [Official Test Plan](HMS_Test_Plan%20%282%29.docx.pdf), [SRS](SRS_Document%20%281%29.docx.pdf), [Architecture and Design Specification](HMS_Architecture_and_Design_Spec%20%283%29.docx.pdf). |

## Method and Stage 3 findings

The assigned cases were executed against Test Plan v1.0 Appendix A using the current dirty worktree at HEAD `010bdf2849c73f0ad26be266f64e6a57d213bef6`. OpenSSL 3.6.5 was dynamically linked. CMake found `OPENSSL_APPLINK_SOURCE` but the MSVC server target omitted `OpenSSL::applink`; the project enabled C++ only, so the C source was not compiled. `CMakeLists.txt` now enables C for this MSVC case and privately links the documented `OpenSSL::applink` target. Fresh build Ninja files show `applink.c.obj` compiled as C and linked into `hms_server.exe`.

TLS verification with the local certificate passed for TLS 1.2 and TLS 1.3; TLS 1.0/1.1, untrusted self-signed certificate validation, and plaintext HTTP were rejected. The server stayed running during negative handshakes. Authenticated synthetic workflows ran against an isolated PostgreSQL 17.11 database on `127.0.0.1:55432`. The ten-minute idle session expired as expected after 606 seconds. The existing PostgreSQL 17 service on port 5432 was not used or changed.

API workflows confirmed user management, password storage, role checks, injection handling, patient ownership, visit creation, encryption at rest, audit-event writes, and session invalidation. Several requested routes returned 404/501 and are recorded as failures or blocked prerequisites. Full output is in [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt). Automated test output is in the fresh build, CTest, and PG integration logs in `../evidence/automated_tests/`.
## Assigned-case summary

| Test Case | Requirement | Description | Status | Evidence |
|---|---|---|---|---|
| TC-APT-05 | HMS-F-024 | Doctor daily appointment schedule | FAIL | [case evidence](../evidence/APT/TC-APT-05/status.txt) |
| TC-CLN-05 | HMS-F-034 | Patient views own records and bills | FAIL | [case evidence](../evidence/CLINICAL/TC-CLN-05/status.txt), [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-BIL-01 | HMS-F-040 | Generate itemised bill | FAIL | [case evidence](../evidence/BILLING/TC-BIL-01/status.txt) |
| TC-BIL-02 | HMS-F-041 | Record payments and update bill status | BLOCKED | [case evidence](../evidence/BILLING/TC-BIL-02/status.txt) |
| TC-BIL-03 | HMS-F-042 | Generate printable receipt | BLOCKED | [case evidence](../evidence/BILLING/TC-BIL-03/status.txt) |
| TC-RPT-01 | HMS-F-050 | Daily appointment and revenue reports | FAIL | [case evidence](../evidence/REPORTS/TC-RPT-01/status.txt) |
| TC-ADM-01 | HMS-F-060 | User management | PASS | [case evidence](../evidence/ADMIN/TC-ADM-01/status.txt), [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-ADM-02 | HMS-F-061 | Audit log filtering | FAIL | [case evidence](../evidence/ADMIN/TC-ADM-02/status.txt) |
| TC-PORT-01 | HMS-NF-007 | Windows and Ubuntu platform compatibility | PARTIAL | [case evidence](../evidence/PORTABILITY/TC-PORT-01/status.txt), [Stage 3 Windows build](../evidence/automated_tests/stage3_fresh_windows_build_test_20261115.log) |
| TC-MNT-01 | HMS-NF-008 | Per-service unit-test coverage | BLOCKED | [case evidence](../evidence/MAINTAINABILITY/TC-MNT-01/status.txt) |
| TC-SEC-01 | HMS-SEC-001 | Password security | PASS | [case evidence](../evidence/SECURITY/TC-SEC-01/status.txt), [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-SEC-02 | HMS-SEC-002 | Transport security | PASS | [case evidence](../evidence/SECURITY/TC-SEC-02/status.txt), [Stage 3 TLS/API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-SEC-03 | HMS-SEC-003 | Session management | PASS | [case evidence](../evidence/SECURITY/TC-SEC-03/status.txt), [Stage 3 idle-expiry evidence](../evidence/automated_tests/stage3_idle_session_20261115.txt) |
| TC-SEC-04 | HMS-SEC-004 | Input validation and injection | PARTIAL | [case evidence](../evidence/SECURITY/TC-SEC-04/status.txt) |
| TC-SEC-05 | HMS-SEC-005, HMS-SEC-008 | Audit logging and sensitive-data exclusion | FAIL | [case evidence](../evidence/SECURITY/TC-SEC-05/status.txt), [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-SEC-06 | HMS-SEC-006 | Data protection at rest | PASS | [case evidence](../evidence/SECURITY/TC-SEC-06/status.txt) |
| TC-SEC-07 | HMS-SEC-007 | Server-side authorisation | PASS | [case evidence](../evidence/SECURITY/TC-SEC-07/status.txt), [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt) |
| TC-AVL-01 | HMS-NF-004 | Seven-day availability | BLOCKED | [case evidence](../evidence/AVAILABILITY/TC-AVL-01/status.txt) |
| TC-REL-01 | HMS-NF-006 | Backup and recovery | BLOCKED | [case evidence](../evidence/RELIABILITY/TC-REL-01/status.txt) |
| TC-USA-01 | HMS-NF-005, HMS-NF-009 | Five-user usability | BLOCKED | [case evidence](../evidence/USABILITY/TC-USA-01/status.txt) |

## Results and metrics

| Status | Count |
|---|---:|
| PASS | 6 |
| FAIL | 6 |
| BLOCKED | 6 |
| PARTIAL | 2 |
| **Total assigned** | **20** |

- Cases with direct assigned-case evidence: **14/20 = 70%** (6 PASS, 6 FAIL, 2 PARTIAL).
- Full-pass rate: **6/20 = 30%**. Six cases remain blocked; two are partial.
- Passed requirement coverage among the assigned cases: **6/20**.
- All six available prebuilt component executables exited 0. This is baseline component evidence, not a 20-case pass count.

## Defects and limitations

The OpenSSL applink runtime defect is fixed and TLS tests now pass. Confirmed functional failures include audit-log filtering (501), the specified doctor schedule endpoint (404; alternate schedule route 501), patient own history and bills (404), bill/payment/receipt and revenue-report endpoints (501), and missing audit coverage for direct patient-record views. PostgreSQL 15, Ubuntu 22.04, module coverage, seven-day availability, backup recovery and five-user usability remain unverified or blocked. See the [defect report](defect_report.md) and [environment guide](test_environment_and_reproduction.md).
This report covers only the assigned 20 cases. It does not establish completion of the project’s 40-case Test Plan, satisfaction of the project-wide 95% pass exit criterion, or overall system acceptance. The results are not ready for submission as completed testing; further execution in a provisioned build/API/database environment is required.

## Final conclusion

The OpenSSL runtime problem is resolved and the local TLS/API is reachable with certificate validation enabled. **Six cases passed, six failed, six are blocked, and two are partial.** This is not ready to submit as completed assignment testing. See [execution details](test_case_execution_details.md) and the case evidence files.
