# Branch Coverage Report — 2026-11-12

## Current working-tree result

**MEASURED — current instrumented Linux run.** Coverage was captured from the
current tree using GCC 13.3, gcov, and lcov with `branch_coverage=1`. All 16
CTest suites passed, with PostgreSQL adapter and backup/restore tests enabled.

| Service module | Covered / executable source branches | Branch coverage |
| --- | ---: | ---: |
| appointments | 758 / 1,517 | 49.97% |
| auth | 693 / 1,279 | 54.18% |
| backup | 445 / 878 | 50.68% |
| billing | 453 / 889 | 50.96% |
| clinical (both clinical libraries grouped) | 415 / 836 | 49.64% |
| health | 63 / 80 | 78.75% |
| password security | 32 / 42 | 76.19% |
| patients | 631 / 1,258 | 50.16% |
| reports | 128 / 230 | 55.65% |
| **Total service-source inventory** | **3,618 / 7,009** | **51.62%** |

These are branch outcomes for the C++ source files listed in the coverage
manifest. They exclude headers, generated dependencies, tests, API, GUI, and
server entry point; no minimum branch threshold is defined by the project.
The generated LCOV files are in the ignored local `build-coverage/coverage/`
directory. The raw `.info` data is not checked into the repository.

Compared with the prior measured result of 3,473 / 7,009 (49.55%), the latest
targeted tests covered 145 additional branch outcomes: appointments increased
by 49, backup by 45, and patients by 51. The new cases exercise PostgreSQL
repository outcomes (including rescheduling/status changes, invalid keys,
linked-user conflicts, and missing records) and backup restore validation.
These are additional tests of real behavior; remaining uncovered outcomes
still need review rather than being treated as an automatic quality failure.
