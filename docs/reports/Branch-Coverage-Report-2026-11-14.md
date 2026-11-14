# Branch Coverage Report — 2026-11-14

## Latest CI result

**MEASURED — branch data was present in the CI LCOV artifact.** Coverage was
captured with GCC/gcov and lcov on Ubuntu 22.04 after enabling LCOV's
`lcov_branch_coverage=1` option. All 16 CTest suites ran with PostgreSQL
repository and backup/restore integration enabled. Source files are limited
to the CMake service-module inventory; clinical libraries are combined.
Evidence is from commit `b170901620c751fd552ba5e1f3a0732be69fab7c` in
[GitHub Actions run 43625440994](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994).

| Service module | Covered / executable source branches | Branch coverage |
| --- | ---: | ---: |
| appointments | 700 / 1,287 | 54.39% |
| auth | 726 / 1,302 | 55.76% |
| backup | 446 / 856 | 52.10% |
| billing | 432 / 795 | 54.34% |
| clinical (both clinical libraries grouped) | 430 / 818 | 52.57% |
| health | 63 / 80 | 78.75% |
| password security | 32 / 42 | 76.19% |
| patients | 620 / 1,130 | 54.87% |
| reports | 133 / 240 | 55.42% |
| **Total service-source inventory** | **3,582 / 6,550** | **54.69%** |

These are branch outcomes for the C++ source files listed in the coverage
manifest. Counts include branch outcomes only for those source files, not
headers, generated dependencies, tests, API, GUI, or the server entry point.
The Test Plan defines no minimum branch-coverage threshold; this is a measured
quality indicator, not a pass/fail criterion. The downloadable
[CI LCOV artifact](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994/artifacts/14125164213)
contains the tracefile used to derive these totals. Uncovered outcomes should
be reviewed in context rather than treated as automatic defects.
