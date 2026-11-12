# Unit Test Report — 2026-11-12

## Summary

| Item | Result |
| --- | --- |
| Build configuration | Existing Visual Studio 17 2022, x64, Release build in `build-vs` |
| Build | PASS — `cmake --build build-vs --config Release --parallel 4` |
| Windows CTest suites (previous run) | 16 passed, 0 failed, 0 skipped |
| Latest Linux CTest suites | 16 passed, 0 failed, 0 skipped, with PostgreSQL adapter/backup integration enabled |
| Latest Linux CTest duration | 20.01 seconds |
| Latest full CTest rerun after low-coverage edge-case additions | 16 passed, 0 failed, 20.01 seconds |
| Coverage-checker tests | 3 passed |
| Qt client startup smoke | PASS — remained running for 4 seconds with `QT_QPA_PLATFORM=offscreen` |

## Windows CTest results

These Windows results predate the latest low-coverage test additions. The
latest additions were compiled and verified in the Linux PostgreSQL-enabled
run below.

Command:

```powershell
ctest --test-dir build-vs -C Release --output-on-failure
```

| Suite | Result |
| --- | --- |
| `backup_service_test` | PASS |
| `api_test` | PASS |
| `auth_service_test` | PASS |
| `health_test` | PASS |
| `password_security_test` | PASS |
| `appointments_test` | PASS |
| `change_appointment_test` | PASS |
| `doctor_schedule_test` | PASS |
| `clinical_service_test` | PASS |
| `report_service_test` | PASS |
| `billing_service_test` | PASS |
| `billing_tax_boundary_test` | PASS |
| `receipt_test` | PASS |
| `patient_service_test` | PASS |
| `patient_field_encryption_test` | PASS |
| `clinical_test` | PASS |

The coverage checker was exercised with:

```powershell
python scripts\test_check_service_coverage.py
```

Result: 3 tests passed (threshold pass, below-threshold diagnostic, and missing
source diagnostic).

## Linux PostgreSQL-enabled CTest results

The current working tree was built with GCC 13.3 and
`HMS_ENABLE_COVERAGE=ON` in WSL Ubuntu 24.04. A local PostgreSQL 16.15 instance
was created for this run, the repository migrations were applied, and all six
PostgreSQL test flags were enabled:

```text
HMS_AUTH_TEST_POSTGRES=1
HMS_PATIENT_TEST_POSTGRES=1
HMS_APPOINTMENT_TEST_POSTGRES=1
HMS_CLINICAL_TEST_POSTGRES=1
HMS_BILLING_TEST_POSTGRES=1
HMS_BACKUP_TEST_POSTGRES=1
```

`HMS_BACKUP_TEST_RESTORE_DATABASE` pointed to a separate disposable database.
The backup test exercised `pg_dump`/`pg_restore`, encrypted backup, restore,
and rejection of a repeated restore into a non-empty target. Fresh disposable
databases were used for this run. CTest result: 16/16 passed, 0 failed in
20.01 seconds.

The report and health tests cover CSV quoting/newline cases, PDF escaping and
empty-report cases, plus availability validation failures, negative UTC offset,
partial final interval, and no-eligible-hours cases. Appointment tests cover
malformed schedule/time cases, invalid doctor and patient IDs, empty next-slot
limits, repository lookup/status outcomes, occupied rescheduling, and cancelled
appointments. Patient tests cover validation outcomes, missing records, linked
user conflicts, invalid repository keys/indexes, and rejection of decryption
with the wrong key. Backup tests cover malformed/empty archives, file and key
errors, empty-file encryption, and restore identifier/confirmation validation.
The full PostgreSQL-enabled suite passed after these additions.

## Limitations

The Windows CTest run did not set PostgreSQL-backed flags; those paths were
covered by the separate Linux run above. Both are automated test evidence, not
evidence that all 40 manual/system Test Plan cases passed. No raw `.xml` test
result bundle was generated.
