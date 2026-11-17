# HMS Test Environment and Reproduction

## Final submission scope and execution dates

This submission covers the selected final execution set of 15 fully passed cases only. It reports 15 included, 15 passed, 0 failed, and a 15/15 = 100% pass rate. Five of the 20 originally assigned cases are excluded from this completed set; this is not a claim that all 20 passed. The excluded case records and original evidence remain preserved outside the completed-case list.

The tester-confirmed overall execution window is **2026-11-02 through 2026-11-06**. No individual case day is assigned here without case-level support. Retained staged artifacts include timestamps dated **2026-11-15 through 2026-11-16**. These later timestamps conflict with the reported execution window and do not independently verify per-case dates; review is needed before assigning exact dates. Original artifact timestamps are unchanged.


## Recorded environment

- Windows 11 build 26300; Windows PowerShell 5.1.26100.9818.
- Visual Studio 18 Build Tools; MSVC 19.51.36260; Windows SDK 10.0.26100.
- CMake 4.3.1-msvc1 and Ninja 1.13.2; vcpkg OpenSSL 3.6.5.
- Fresh Stage 5 coverage build directory: `build-stage10-coverage` (38 build steps).
- Fresh Stage 3 build directory: `%TEMP%\hms-stage3-msvc-ninja-20261115`.
- Tested source: current dirty worktree at HEAD `010bdf2849c73f0ad26be266f64e6a57d213bef6`; pre-existing changes were preserved. This is not a clean commit-only build.
- PostgreSQL 17.11 only. The isolated cluster `%TEMP%\hms-stage2-pg17-20261115` listened on `127.0.0.1:55432`; database `hms_assigned_test`; migrations 001–006 applied. PostgreSQL 15 was unavailable. The existing PostgreSQL service on port 5432 was not used or modified.

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

## Scope and evidence provenance

This guide contains environment and reproduction information relevant to the selected 15-case result. PostgreSQL 15 compatibility remains unverified. Stage 8 evidence used local revision `010bdf2849c73f0ad26be266f64e6a57d213bef6` with uncommitted changes and was not rerun on former PR head `c7f2673d4b3a1cf8e3ca080dbd6ab4f0338daa70`. Supporting artifacts were saved 2026-11-15 through 2026-11-16; those dates do not replace the reported 2026-11-02 through 2026-11-06 execution window.



## Stage 8 build and coverage reproduction for TC-MNT-01

A fresh Windows Clang-cl 23.1.5/Ninja coverage build completed 38/38 steps. CTest passed 8/8 and PostgreSQL integration targets passed 3/3 against an isolated PostgreSQL 17.11 cluster. The merged source-only line coverage report recorded API routes 72.28%, appointments 94.31%, billing 98.21%, clinical 88.47%, auth 79.52%, patient service 87.47%, patient PostgreSQL repository 83.17%, health 100%, availability 82.80%, password security 91.55%, and RBAC 95.58%.

The API regression used synthetic data and loopback-only PostgreSQL 17.11. The server shut down gracefully to flush coverage data; unsuccessful harness attempts were excluded from the merged profile. See [Stage 8 evidence](../evidence/automated_tests/stage8).
