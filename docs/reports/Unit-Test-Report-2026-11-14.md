# Unit Test Report — 2026-11-14

## Summary

| Item | Result |
| --- | --- |
| Application commit | `18928eb` (`fix: align payment API with SAD route`) |
| Linux build/test CI | PASS — Ubuntu 22.04 build and all 16 CTest suites |
| Windows build/test CI | PASS — Windows Server 2022 build and all 16 CTest suites |
| PostgreSQL-enabled coverage-job tests | PASS — all 16 CTest suites, with the configured repository and backup integration flags |
| Local WSL rerun | PASS — 16/16 CTest suites in 12.06 seconds |
| Coverage-checker unit tests | PASS — 3 tests |
| Qt client startup smoke | PASS — remained active for the bounded 4-second offscreen check |
| Branch-capture workflow-fix CI | PASS — coverage job ran all 16 suites and uploaded LCOV with branch data for commit `b170901` |

The local WSL rerun used the existing GCC 13.3 coverage build. Its PostgreSQL
integration flags were not enabled, so the PostgreSQL-enabled CI run is the
evidence for repository and backup integration cases. The latest complete
Windows/Ubuntu build and test matrix is
[GitHub Actions run 43625440994](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994).

## Test scope and limitations

The 16 registered CTest suites cover authentication, API helpers, health,
password security, appointments, doctor schedules, clinical records, reports,
billing, receipts, patient services, encryption, and backups. These component
and integration tests are not a one-to-one execution of the complete manual
Test Plan.

The CI branch-capture correction changes only coverage reporting; it does not
change application code. Its coverage job passed and produced the branch data
reported in [Branch-Coverage-Report-2026-11-14.md](Branch-Coverage-Report-2026-11-14.md).
The Windows/Ubuntu application build and test and corrected coverage results
are from run 43625440994. No claim is made here
that live TLS routes, all manual Test Plan cases, production-scale load,
availability, participant usability, or a production recovery drill passed.
