# Code Coverage Report — 2026-11-12

## Current working-tree result

**PASS — all service modules meet the 70% executable-line threshold.** The
current tree was built with GCC 13.3 and `HMS_ENABLE_COVERAGE=ON` in WSL
Ubuntu 24.04. All 16 CTest suites passed with PostgreSQL-backed repository
tests and backup/restore enabled. The service-module checker read the current
LCOV data and passed its 70% per-module line threshold:

| Service module | Covered / executable source lines | Line coverage |
| --- | ---: | ---: |
| appointments | 594 / 635 | 93.54% |
| auth | 633 / 726 | 87.19% |
| backup | 77 / 77 | 100.00% |
| billing | 342 / 369 | 92.68% |
| clinical (both clinical libraries grouped) | 369 / 392 | 94.13% |
| health | 78 / 78 | 100.00% |
| password security | 44 / 47 | 93.62% |
| patients | 575 / 628 | 91.56% |
| reports | 93 / 94 | 98.94% |

Across the CMake service-source inventory, 2,805 of 3,046 executable lines
were covered (92.09%). The API, GUI, and server entry point are excluded as
described in `docs/coverage.md`; this result does not measure those surfaces.

The generated LCOV files are in the ignored local `build-coverage/coverage/`
directory. The coverage checker output and test result were observed during
this run; generated raw `.info` files are not checked into the repository.
