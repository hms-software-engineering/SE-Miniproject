# System Validation Report — 2026-11-14

## Outcome

**PARTIAL — system acceptance is not complete.** The current Windows Release
and Linux builds passed in CI, all 16 registered CTest suites passed on both
platforms, the PostgreSQL-enabled coverage job passed its integration tests,
and the Qt desktop client stayed running during a bounded 4-second offscreen
startup check. These checks do not validate live API routes or the complete
manual Test Plan.

## Environment and evidence

| Check | Result | Evidence |
| --- | --- | --- |
| Windows 2022 build and CTest | PASS | GitHub Actions run 43625440994; all 16 CTest suites passed |
| Ubuntu 22.04 build and CTest | PASS | GitHub Actions run 43625440994; all 16 CTest suites passed |
| PostgreSQL adapter/backup integration | PASS | Coverage job in run 43625440994 ran all 16 suites with integration flags enabled |
| Updated line and branch coverage capture | PASS | Run 43625440994 uploaded the branch-enabled LCOV artifact; see the code and branch coverage reports |
| Local WSL CTest rerun | PASS | GCC 13.3 build; 16/16 passed in 12.06 seconds (PostgreSQL flags not enabled locally) |
| Qt client launch | PASS | Linux offscreen client remained active throughout the bounded 4-second smoke window |
| TLS API endpoint and role workflow validation | NOT RUN | No live HMS server was available in the reporting session |
| Security scanner / end-to-end authorization | NOT RUN | Requires an approved isolated staging deployment and test accounts |
| Performance, availability, five-user usability, recovery drill | NOT RUN | Requires the Test Plan's environment, synthetic data, participants, monitoring, and timed drill |

## Test Plan disposition

The CTest run provides component-level evidence for portions of authentication,
patients, appointments, clinical records, billing, reports, health, backup,
validation, and password security. It is not a one-to-one execution of the
40-case Appendix A.

**Passed in the scope actually executed:** Windows and Ubuntu builds; 16 CTest
suites on both platforms; 16 CTest suites in the PostgreSQL-enabled coverage
job; coverage-checker unit tests; Qt client startup.

**Not claimed as passed:** live login/bootstrap administrator; route-level
system validation; TLS scanner; complete all-role authorization matrix;
100k-patient search benchmark; 100-user appointment benchmark; current
200-session rerun; availability measurement; five-participant usability;
production-scale backup/recovery drill; MSVC 2019/Ubuntu portability matrix.

Use the execution checklist and updated oracles in `docs/test-cases-updated.md`.
Any failures discovered during the remaining system tests should be logged as
defects with reproduction steps and synthetic test data only.
