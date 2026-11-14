# System Validation Report — 2026-11-14

**Evidence snapshot:** CI and local test runs recorded through 2026-11-14.
**Documentation revision:** 2026-11-15.

## Sprint verification summary

The recorded sprint verification passed: Windows and Ubuntu builds, all 16
registered CTest suites on both platforms, the PostgreSQL-enabled coverage
job, and a bounded four-second Qt client startup smoke check. These results
provide automated and startup evidence for the sprint deliverables.

The project owner additionally reports manual login and workflow testing.
Those manual tests were not supplied as a case-by-case execution log, so this
report does not attribute specific outcomes to individual Test Plan cases or
claim that the full 40-case manual plan was completed. The items below identify
acceptance evidence not present in the submitted test record; they do not
assert that the corresponding application features are missing.

## Environment and evidence

| Check | Result | Evidence |
| --- | --- | --- |
| Windows 2022 build and CTest | PASS | GitHub Actions run 43625440994; all 16 CTest suites passed |
| Ubuntu 22.04 build and CTest | PASS | GitHub Actions run 43625440994; all 16 CTest suites passed |
| PostgreSQL adapter/backup integration | PASS | Coverage job in run 43625440994 ran all 16 suites with integration flags enabled |
| Updated line and branch coverage capture | PASS | Run 43625440994 uploaded the branch-enabled LCOV artifact; see the code and branch coverage reports |
| Local WSL CTest rerun | PASS | GCC 13.3 build; 16/16 passed in 12.06 seconds (PostgreSQL flags not enabled locally) |
| Qt client launch | PASS | Linux offscreen client remained active throughout the bounded 4-second smoke window |
| Live TLS/API and complete role authorization matrix | No result recorded | Automated TLS setup and authorization code/tests are documented elsewhere; no live route/matrix execution evidence was supplied |
| Security scanner / end-to-end authorization | No result recorded | No scanner or isolated-staging execution evidence was supplied |
| 100k-patient search and 100-user appointment performance targets | Not included in this sprint execution record | The project owner considers the large-data workload out of scope for this submission; no benchmark result is claimed |
| 200-session performance target | Historical result only | `tests/performance/NF-003-summary.txt` contains the prior run; no current rerun is claimed |
| Availability target | No result recorded | No timestamped service-window measurement was supplied |
| Five-participant usability protocol | No result recorded | No participant-level completion data was supplied; implemented UI behavior alone does not demonstrate the threshold |
| Timed production-scale backup/recovery drill | No result recorded | Automated backup tests passed, but no timed operational drill evidence was supplied |
| Detailed platform acceptance matrix | Partially evidenced in CI | Ubuntu 22.04 and Windows Server 2022/MSVC 2022 passed in CI; the owner reports portability checks, but detailed OS/compiler results were not included |

## Test Plan disposition

The CTest run provides component-level evidence for portions of authentication,
patients, appointments, clinical records, billing, reports, health, backup,
validation, and password security. It is not a one-to-one execution of the
40-case Appendix A.

**Passed in the scope actually executed:** Windows and Ubuntu builds; 16 CTest
suites on both platforms; 16 CTest suites in the PostgreSQL-enabled coverage
job; coverage-checker unit tests; Qt client startup.

The report claims only the checks supported by the evidence above. Additional
manual app use reported by the project owner is acknowledged, but without a
case-by-case record it cannot establish the remaining Test Plan thresholds.

Use the execution checklist and updated oracles in `docs/test-cases-updated.md`.
Any failures discovered during the remaining system tests should be logged as
defects with reproduction steps and synthetic test data only.
