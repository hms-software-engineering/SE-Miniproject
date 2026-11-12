# System Validation Report — 2026-11-12

## Outcome

**PARTIAL — system acceptance is not complete.** The current Windows Release
build succeeded, all 16 registered CTest suites passed, the database-backed
Linux integration tests passed, and the Qt desktop client stayed running
during a 4-second offscreen startup smoke check. These checks do not validate
the live API routes or the full Test Plan.

## Environment and evidence

| Check | Result | Evidence |
| --- | --- | --- |
| Build current configured Windows x64 Release targets | PASS | `cmake --build build-vs --config Release --parallel 4` exited successfully |
| Automated Windows CTest suites | PASS | 16/16 passed; see `Unit-Test-Report-2026-11-12.md` |
| Linux build and PostgreSQL-backed CTest suites | PASS | GCC 13.3 build succeeded; 16/16 tests passed with repository and backup integration flags enabled against PostgreSQL 16.15 |
| PostgreSQL schema setup | PASS | All seven repository migrations applied to a disposable local database |
| Encrypted database backup and restore | PASS | Integration test used `pg_dump`/`pg_restore`, restored to a separate empty test database, and verified a second restore was rejected |
| Qt client launch | PASS | `hms_client.exe` remained active for 4 seconds with Qt offscreen; smoke process then stopped |
| TLS API endpoint and role workflow validation | NOT RUN | No live HMS server was available in the reporting session |
| Security scanner / end-to-end authorization | NOT RUN | Requires an approved isolated staging deployment and test accounts |
| Performance, availability, five-user usability, recovery drill | NOT RUN | Requires the Test Plan's environment, synthetic data, participants, monitoring, and timed drill |

## Test Plan disposition

The CTest run provides component-level evidence for portions of authentication,
patients, appointments, clinical records, billing, reports, health, backup,
validation, and password security. It is not a one-to-one execution of the
40-case Appendix A.

**Passed in the scope actually executed:** current build; 16 Windows CTest
suites; 16 Linux CTest suites with PostgreSQL integrations; coverage-checker
unit tests; Qt client startup.

**Not claimed as passed:** live login/bootstrap administrator; route-level
system validation; TLS scanner; complete all-role authorization matrix;
100k-patient search benchmark; 100-user appointment benchmark; current
200-session rerun; availability measurement; five-participant usability;
production-scale backup/recovery drill; MSVC 2019/Ubuntu portability matrix.

Use the execution checklist and updated oracles in `docs/test-cases-updated.md`.
Any failures discovered during the remaining system tests should be logged as
defects with reproduction steps and synthetic test data only.
