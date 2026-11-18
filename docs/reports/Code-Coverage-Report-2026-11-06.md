# Code Coverage Report — 2026-11-14

## Latest CI result

**PASS — all service modules meet the 70% executable-line threshold.** The
coverage job ran with GCC and lcov on Ubuntu 22.04, with all 16 CTest suites
and PostgreSQL-backed repository/backup tests enabled. The report artifact
was captured from commit `b170901620c751fd552ba5e1f3a0732be69fab7c` in
[GitHub Actions run 43625440994](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994).

| Service module | Covered / executable source lines | Line coverage |
| --- | ---: | ---: |
| appointments | 593 / 634 | 93.53% |
| auth | 676 / 785 | 86.11% |
| backup | 77 / 77 | 100.00% |
| billing | 339 / 366 | 92.62% |
| clinical (both clinical libraries grouped) | 371 / 395 | 93.92% |
| health | 80 / 80 | 100.00% |
| password security | 44 / 47 | 93.62% |
| patients | 575 / 629 | 91.41% |
| reports | 93 / 94 | 98.94% |

Across the CMake service-source inventory, 2,848 of 3,107 executable lines
were covered (91.66%). Every module exceeds the 70% requirement. The API, GUI,
and server entry point are excluded as described in `docs/coverage.md`; this
result does not measure those surfaces.

The [CI LCOV artifact](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994/artifacts/14125164213)
contains the source tracefile used for the line and branch reports. The
workflow runs `scripts/check_service_coverage.py` against this data and fails
if any module is below 70%. No coverage threshold is specified for the GUI,
API, or overall branch percentage.
