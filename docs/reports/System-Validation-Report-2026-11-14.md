# System Validation Report — 2026-11-17

**Evidence snapshot:** Test execution evidence and CI/local verification records available through 2026-11-06.

**Documentation revision:** 2026-11-06.

## 1. Testing Scope

The testing activities covered 29 test cases from the HMS Software Test Plan. These cases cover administration, appointments, authentication, billing, clinical records, patient management, platform compatibility, reporting, security and usability.

The test execution evidence is organized under `Testing/Test_Evidence/Screenshots_logs/`, with separate folders for the corresponding test-case IDs.

### Test Cases Covered

| Module | Test Cases | Count |
| --- | --- | ---: |
| Administration | TC-ADM-01, TC-ADM-02 | 2 |
| Appointments | TC-APT-01, TC-APT-02, TC-APT-03, TC-APT-05 | 4 |
| Authentication | TC-AUTH-01, TC-AUTH-02 | 2 |
| Billing | TC-BIL-01, TC-BIL-02, TC-BIL-03 | 3 |
| Clinical Records | TC-CLN-01, TC-CLN-02, TC-CLN-05 | 3 |
| Patient Management | TC-PAT-01, TC-PAT-02, TC-PAT-03, TC-PAT-04, TC-PAT-05 | 5 |
| Portability | TC-PORT-01 | 1 |
| Reporting | TC-RPT-01 | 1 |
| Security | TC-SEC-01, TC-SEC-02, TC-SEC-03, TC-SEC-04, TC-SEC-05, TC-SEC-06, TC-SEC-07 | 7 |
| Usability | TC-USA-01 | 1 |
| **Total** | | **29** |

## 2. Environment and Evidence

| Check | Result | Evidence |
| --- | --- | --- |
| Windows local build and testing | PASS | Existing Visual Studio build; 16/16 CTest suites passed |
| Ubuntu 22.04 build and CTest | PASS | GitHub Actions run 43632305410; all 16 CTest suites passed |
| Windows Server 2022 build and CTest | PASS | GitHub Actions run 43632305410; all 16 CTest suites passed |
| PostgreSQL integration testing | PASS | PostgreSQL-enabled coverage job in GitHub Actions run 43632305410; all 16 CTest suites passed |
| Local API liveness and readiness | PASS | Existing HMS server returned HTTP 200 for both endpoints; readiness reported PostgreSQL `ok` |
| TLS and plaintext transport | PASS | Trusted TLS 1.2 handshake succeeded; HTTP request to the TLS-only port received no HTTP response |
| Unauthenticated user listing | PASS | `GET /api/v1/users` returned HTTP 401 |
| Code coverage | PASS | Updated line coverage report generated; see the code coverage report |
| Branch coverage | PASS | Branch-enabled LCOV artifact from GitHub Actions run 43625440994 |
| Qt client startup | PASS | Linux offscreen client remained active throughout the bounded four-second startup smoke test |

The local API checks used the already-running HMS server and did not modify application data. Starting a second server instance was unsuccessful because PostgreSQL reported `too many clients already`. The existing server was retained for the read-only checks.

## 3. Automated Test Verification

The automated verification included:

- Windows and Ubuntu builds.
- Execution of all 16 registered CTest suites on both platforms.
- PostgreSQL-enabled integration testing through the coverage job.
- Updated line and branch coverage capture.
- Qt client startup smoke testing.

The Windows and Ubuntu CTest runs completed successfully, with all 16 registered suites passing on each platform.

These automated results provide supporting evidence for the tested application components but do not represent a one-to-one execution of all 29 system test cases.

## 4. Test Execution Evidence

Individual test execution evidence is maintained in the following directory:

`Testing/Test_Evidence/Screenshots_logs/`

The directory contains separate evidence folders for all 29 test cases listed in Section 1.

Supporting artifacts include:

- Screenshots of application workflows and test outcomes.
- Database verification results where applicable.
- API responses and security verification evidence.
- Code coverage and branch coverage reports.
- Automated test execution results.

## 5. Test Results and Disposition

The 29 test cases listed in this report represent the documented testing scope for this submission.

Each test case should be evaluated according to its recorded execution result:

- **PASS:** Actual result satisfies the expected result.(29)
- **FAIL:** Actual result does not satisfy the expected result.(0)
- **BLOCKED:** Testing could not be completed because of an external dependency or environment limitation.(0)
- **NOT RUN:** Testing was not performed.(0)

The presence of an evidence folder alone does not establish a PASS result. Individual outcomes are recorded in the corresponding test execution records.

## 6. Conclusion

The HMS project underwent functional, authentication, security, database, reporting, portability and usability-related testing across the documented test scope.

Automated verification demonstrated successful builds and execution of all 16 registered CTest suites on Windows and Ubuntu, along with PostgreSQL-enabled integration testing and coverage collection.

The 29 documented test cases are supported by their corresponding evidence folders, while automated CI and local verification artifacts provide additional validation of the application.

The final test disposition is based on the actual results recorded in the individual test execution logs.