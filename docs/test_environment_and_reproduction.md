# HMS Test Environment and Reproduction

## Recorded environment

- Windows 11 build 26300; Windows PowerShell 5.1.26100.9818.
- Visual Studio 18 Build Tools; MSVC 19.51.36260; Windows SDK 10.0.26100.
- CMake 4.3.1-msvc1 and Ninja 1.13.2; vcpkg OpenSSL 3.6.5.
- Fresh Stage 5 coverage build directory: `build-stage10-coverage` (38 build steps).
- Fresh Stage 3 build directory: `%TEMP%\hms-stage3-msvc-ninja-20261115`.
- Tested source: current dirty worktree at HEAD `010bdf2849c73f0ad26be266f64e6a57d213bef6`; pre-existing changes were preserved. This is not a clean commit-only build.
- Qt6 was not found, so the Windows client target was skipped.
- PostgreSQL 17.11 only. The isolated cluster `%TEMP%\hms-stage2-pg17-20261115` listened on `127.0.0.1:55432`; database `hms_assigned_test`; migrations 001–006 applied. PostgreSQL 15 was unavailable. The existing PostgreSQL service on port 5432 was not used or modified.
- Ubuntu 22.04 was unavailable: WSL returned `E_ACCESSDENIED`, and no Ubuntu app was installed.

## Build and automated tests

The fresh Stage 5 clang-cl/Ninja build completed 38/38 steps, including OpenSSL Applink, daily appointment exports and the PDF leading fix. CTest passed 8/8; PostgreSQL-enabled CTest passed 8/8 against isolated PostgreSQL 17.11. Stage 5 API checks and viewer inspection of the receipt and revenue exports completed. Coverage build, CTest and PostgreSQL CTest logs are in [coverage evidence](../evidence/MAINTAINABILITY/TC-MNT-01); the API artifacts are under [automated test evidence](../evidence/automated_tests).

Evidence: [Stage 4 clean build](../evidence/automated_tests/stage4_build_20261115.log), [Stage 4 CTest](../evidence/automated_tests/stage4_ctest_20261115.log), [Stage 4 PostgreSQL integration CTest](../evidence/automated_tests/stage4_postgresql_ctest_20261115.log), and [database compatibility record](../evidence/automated_tests/stage3_database_compatibility_20261115.txt).

Stage 4 assigned API verification is reproducible with:

```powershell
$env:HMS_TEST_BUILD_DIR = (Resolve-Path .\build-stage8-final).Path
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\evidence\automated_tests\run_stage4_api_checks_20261115.ps1
```

The Stage 4 runner uses the isolated PostgreSQL 17.11 cluster at `127.0.0.1:55432`, requires the local TLS probe from Stage 3, generates synthetic user credentials at runtime, records sanitized evidence, and stops its server process. See [Stage 4 API results](../evidence/automated_tests/stage4_api_execution_20261115.txt), [CTest](../evidence/automated_tests/stage4_ctest_20261115.log), and [PostgreSQL CTest](../evidence/automated_tests/stage4_postgresql_ctest_20261115.log).

## TLS and API reproduction

From the repository root in Windows PowerShell, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\evidence\automated_tests\run_stage3_api_checks_native_20261115.ps1
```

The script uses a local OpenSSL client probe, validates the repository certificate as a CA for the loopback hostname, starts the server against the isolated synthetic database, exercises TLS and API cases, records sanitized results, and stops its server process. It does not disable certificate verification. Results are in [Stage 3 API evidence](../evidence/automated_tests/stage3_api_execution_20261115.txt).

The ten-minute idle-expiry run is reproducible with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\evidence\automated_tests\run_stage3_idle_session_test_20261115.ps1
```

It left a session unused for 606 seconds and observed HTTP 401 on the next authenticated request. See [idle-expiry evidence](../evidence/automated_tests/stage3_idle_session_20261115.txt).

## Outstanding procedures

PostgreSQL 15 comparison and Ubuntu 22.04 build remain unavailable. Per-service coverage was captured but the 70% requirement remains unmet for appointments, API, billing and clinical. Seven-day availability monitoring and five-user usability sessions remain unperformed. TC-REL-01 remains blocked because the daily scheduled backup job is not configured, despite a successful manual encrypted isolated restore drill. Case-level results and remaining partials are detailed in the [execution log](test_execution_log.md).

## Stage 6 environment and repeatable results

The Stage 6 clean build used %TEMP%\\hms-stage6-fresh-build-20261116, Ninja, LLVM clang-cl 23.1.5, the Visual Studio Build Tools environment, OpenSSL 3.6.5 and the existing vcpkg-installed dependencies. CMake configured the repository's current dirty worktree; it did not use a clean commit-only checkout. Qt6 was not found, so hms_client was skipped. The build completed 38/38. Normal CTest and PostgreSQL-enabled CTest each passed 8/8. Exact build and result logs are under [Stage 6 evidence](../evidence/automated_tests/stage6_20261115).

The assigned API regression used isolated PostgreSQL 17.11 at 127.0.0.1:55432, database hms_stage6_api_run2, and synthetic users/patients created through the API with random keys generated at runtime. The final DB applied migrations 001, 002, and 004-006. Migration 003 was intentionally omitted for this clean API test because it inserts documented development placeholders that are not valid encrypted patient fields; a separate temporary database retained the seeded test run and was not dropped. No existing PostgreSQL service/database was modified. See [API setup](../evidence/automated_tests/stage6_20261115/postgresql_api_run2_setup.txt), [API result](../evidence/automated_tests/stage6_20261115/stage6_api_execution.txt), and [PostgreSQL CTest](../evidence/automated_tests/stage6_20261115/fresh_postgresql_ctest.log).

Coverage can be reproduced by using the captured CMake configuration [command](../evidence/automated_tests/stage6_20261115/fresh_coverage_configure_command.txt), loading the Visual Studio developer environment, and running CTest against the fresh build. The merged LLVM line coverage report is [coverage_per_module.txt](../evidence/automated_tests/stage6_20261115/coverage_per_module.txt). It shows appointments 61.46%, billing 37.14%, clinical 6.70%; the last usable route coverage (src/main.cpp) is 57.78%. The instrumented API server was force-stopped by the regression harness before profile flush; do not infer route coverage from the helper src/api.cpp 100% line result.

The backup drill is reproducible with powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\\evidence\\automated_tests\\stage6_20261115\\run_backup_restore_test.ps1. It creates a new unique loopback-only PostgreSQL 17 cluster and separate encrypted archive path under %TEMP%, tests restore into a separate disposable database, verifies a fingerprint and failure/no-publish behavior, and stops only the cluster it created. The archive and cluster data are retained under temporary paths for evidence review. The daily task installer is not invoked without real application database paths and external credential configuration. See [backup result](../evidence/automated_tests/stage6_20261115/backup_restore_execution.txt).

The seven-day monitor validates HTTPS certificate chains and PostgreSQL readiness on /api/v1/health, records one-minute checks and missed slots to append-only CSV, and can resume through scheduled startup. It was not registered or run because no trusted staging HTTPS URL or persistent application environment was supplied. Five actual trained receptionists were unavailable, so no timings or success rates were fabricated. Ubuntu 22.04/GCC 11, MSVC 2019, PostgreSQL 15, Qt6 rendering, ZAP, and sqlmap were not available; TC-PORT-01, TC-SEC-04, TC-AVL-01, and TC-USA-01 retain their PARTIAL/BLOCKED statuses.

## Stage 7 recheck (2026-11-16)

No application logic or test procedures changed in this recheck, so Stage 6 remains the latest verified assigned-case result: 14 PASS, 1 FAIL, 3 BLOCKED, and 2 PARTIAL (70%). No case status is promoted. The Stage 7 machine check confirmed Qt6 is unavailable, WSL cannot start because virtualization prerequisites are unavailable, and the existing CI Windows job uses MinGW rather than MSVC 2019. The CI workflow was not triggered because the tested dirty source state is not pushed. Clinical coverage remains substantially below threshold (6.70%), so no low-value mock assertions were added. The present blockers and required external inputs are recorded in [Stage 7 status recheck](../evidence/automated_tests/stage7/status_recheck.txt).

## Stage 8 fresh coverage and API reproduction (2026-11-16)

A fresh Clang-cl 23.1.5/Ninja instrumented build is in `%TEMP%\hms-stage8-build\fresh-coverage-20261116`. The captured configure/build logs and normal CTest (8/8) are in [Stage 8 evidence](../evidence/automated_tests/stage8). The PostgreSQL integration harness creates and stops a unique temporary PostgreSQL 17.11 cluster, applies migrations 001, 002, 004, 005, and 006, and runs clinical/appointment/billing integration tests on synthetic data; the Stage 8 API harness likewise uses an isolated loopback PostgreSQL 17 cluster and runtime-generated synthetic credentials/keys. The complete API run and retained database setup record are [here](../evidence/automated_tests/stage8/api_execution.txt) and [here](../evidence/automated_tests/stage8/api_database_setup.txt). The API harness shuts down the instrumented server through its test stdin control, waits for exit, and records `ServerStoppedGracefully=true`, so LLVM writes the profile.

The Stage 8 profile merge includes successful normal CTest, isolated PostgreSQL service integrations, and the final successful API run. Failed API harness attempts were excluded. Per-source `llvm-cov report` output is in [coverage_per_module.txt](../evidence/automated_tests/stage8/coverage_per_module.txt); the tool source filter was applied separately to each module executable to avoid including unrelated linked files. All applicable reported modules meet >=70% line coverage.

## Stage 8 external requirements still outstanding

No PostgreSQL 15 cluster, Ubuntu 22.04/GCC 11, or MSVC 2019 toolset was available, so their compatibility procedures remain unverified. Qt6 was unavailable, so actual desktop XSS rendering was not performed. No real staging HTTPS endpoint was available for a seven-day monitor, no configured HMS staging database plus approved separate backup destination/credentials was available for scheduled backup, and no five trained receptionist participants were available. The existing usability instructions and result templates remain prepared; no human results were fabricated. Current assigned-case total is 15 PASS, 0 FAIL, 3 BLOCKED, 2 PARTIAL (75%).
## Stage 9 readiness and preparation (2026-11-16)

No C++ source or Stage 8 service/API module changed in Stage 9. Local discovery found Visual Studio Build Tools 18/MSVC toolset 14.51.36231, no v142 toolset, an installed but non-startable Ubuntu WSL distribution (WSL2 virtualization prerequisites disabled), and no Qt6/qmake installation. Existing CI uses Ubuntu 22.04 and Windows MSYS2/MinGW; it was not run because the tested Stage 9 worktree is dirty and the user prohibited pushing. See [readiness record](../evidence/automated_tests/stage9/readiness_assessment.md).

The seven-day monitor remains `evidence/automated_tests/stage6_20261115/Invoke-HmsSevenDayReadinessMonitor.ps1`. It appends probe results and resumes by checking prior scheduled local timestamps. Stage 9 added adjacent `Get-HmsSevenDayAvailabilitySummary.ps1`, which calculates successful unique slots divided by 6,720 expected one-minute slots in `[06:00,22:00)` local for seven days; missing slots count as failures. Reproduce its focused synthetic check with `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\evidence\automated_tests\stage9\Test-HmsAvailabilitySummary.ps1`. The result is in [JSON](../evidence/automated_tests/stage9/availability_calculation_test.json) and [test record](../evidence/automated_tests/stage9/availability_calculation_test.json.test.txt). This does not replace real staging uptime.

TC-USA-01 materials are under [Stage 9 usability preparation](../evidence/automated_tests/stage9/usability/). They contain fictional patient inputs and blank forms; recruit five trained receptionists and use an isolated disposable database before collecting results. TC-REL-01 still needs real staging connection settings, an external protected credential file, approved storage independent from the DB data directory, and the scheduled-task account/DPAPI recovery plan. Neither blocked procedure was started.
## Stage 10 platform and client availability (2026-11-16)

The current CI workflow's Linux matrix entry is `ubuntu-22.04`; Windows runs MSYS2/MinGW. The local Build Tools installation exposes MSVC toolset 14.51 and no v142 toolset. WSL cannot start because required Windows components/virtualization are disabled. CI was not triggered because this worktree contains uncommitted changes and Stage 10 prohibits publishing them. The available public run observed was PR #110 and is unrelated to this worktree.

The optional `hms_client` target requires Qt6 `Widgets` and `Network`; CMake specifies no explicit Qt6 minimum version. The vcpkg manifest contains no Qt dependency, and no Qt/qmake installation was found. The client was not built/launched and no desktop XSS screenshot or dynamic scan was produced. Use a project-approved Qt6 development package and graphical Windows session before attempting the official procedure. Detailed results and external requirements: [Stage 10 evidence](../evidence/automated_tests/stage10/environment_and_results.md).