# HMS Test Environment and Reproduction

## Recorded environment

- Windows 11 build 26300; Windows PowerShell 5.1.26100.9818.
- Visual Studio 18 Build Tools; MSVC 19.51.36260; Windows SDK 10.0.26100.
- CMake 4.3.1-msvc1 and Ninja 1.13.2; vcpkg OpenSSL 3.6.5.
- Fresh Stage 3 build directory: `%TEMP%\hms-stage3-msvc-ninja-20261115`.
- Tested source: current dirty worktree at HEAD `010bdf2849c73f0ad26be266f64e6a57d213bef6`; pre-existing changes were preserved. This is not a clean commit-only build.
- Qt6 was not found, so the Windows client target was skipped.
- PostgreSQL 17.11 only. The isolated cluster `%TEMP%\hms-stage2-pg17-20261115` listened on `127.0.0.1:55432`; database `hms_assigned_test`; migrations 001–006 applied. PostgreSQL 15 was unavailable. The existing PostgreSQL service on port 5432 was not used or modified.
- Ubuntu 22.04 was unavailable: WSL returned `E_ACCESSDENIED`, and no Ubuntu app was installed.

## Build and automated tests

The fresh build completed 33/33 steps, including compiling and linking OpenSSL Applink. CTest passed 7/7 with the vcpkg DLL directory on `PATH`. PostgreSQL-enabled integration CTest passed 7/7 against the isolated PostgreSQL 17 database.

Evidence: [fresh build](../evidence/automated_tests/stage3_fresh_windows_build_test_20261115.log), [CTest](../evidence/automated_tests/stage3_ctest_20261115.log), [PostgreSQL integration CTest](../evidence/automated_tests/stage3_postgresql_ctest_20261115.log), and [database compatibility record](../evidence/automated_tests/stage3_database_compatibility_20261115.txt).

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

PostgreSQL 15 comparison, Ubuntu 22.04 build, per-service coverage, seven-day availability monitoring, backup/restore, and five-user usability sessions were not completed. No compatible coverage tool was available in the environment. Case-level missing routes and outcomes are detailed in the [execution log](test_execution_log.md).
