# HMS Test Execution Log — Daksha SG

## Run metadata

| Field | Observed value |
|---|---|
| Tester | Daksha SG |
| Execution timestamp | 2026-11-05 |
| Commit | `bbb6006d4aa46ce1053acff86f79ae858b196f26` |
| Operating system | `Darwin Dakshas-MacBook-Air 25.1.0 ... RELEASE_ARM64_T8112 arm64` |
| Build command | `cmake --build build --parallel 4` |
| Test command | `ctest --test-dir build --output-on-failure -V` |
| Automated result | 16/16 CTest tests passed; total time 4.86 seconds |

The complete metadata, build output, verbose CTest output, and per-case
executable logs are stored under
[`evidence/automated_tests/`](../../evidence/automated_tests/). The custom
executables are assertion binaries, not GoogleTest binaries; `--gtest_filter`
is unsupported. Each automated case therefore records the exact executable
command that was run and its actual stdout.

Manual demonstration evidence is linked from
[`docs/Sprint_Video_links.txt`](../Sprint_Video_links.txt). The repository
does not contain per-case video timestamps, so the video links are retained as
shared demonstration evidence rather than inventing timestamps.

## Per-case records

All functional/security cases used synthetic fixtures only. No passwords,
tokens, or real patient information are included.

| Case | Setup and steps performed | Expected result | Actual observed result | Evidence | Outcome |
|---|---|---|---|---|---|
| TC-AUTH-01 | Synthetic enabled-user fixture. Ran `./build/auth_service_test`. Reviewed the authentication demonstration. | Valid authentication succeeds. | Executable printed `All authentication and RBAC tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-AUTH-01/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-AUTH-02 | Synthetic valid/invalid credential fixtures. Ran `./build/auth_service_test`. Reviewed the invalid-login demonstration. | Invalid credentials produce generic error behavior. | Executable printed `All authentication and RBAC tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-AUTH-02/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-AUTH-03 | Synthetic failed-attempt fixture. Ran `./build/auth_service_test`. Reviewed the lockout demonstration. | Lockout occurs after the configured failed-attempt threshold. | Executable printed `All authentication and RBAC tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-AUTH-03/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-RBAC-01 | Synthetic Administrator, Doctor, Receptionist, and Patient role fixtures. Ran `./build/auth_service_test`. Reviewed role/permission demonstration. | Authorized actions succeed and unauthorized actions are rejected. | Executable printed `All authentication and RBAC tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-RBAC-01/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-PAT-01 | Synthetic patient fixture. Ran `./build/patient_service_test`. Reviewed registration demonstration. | Valid patient registration succeeds. | Executable printed `All patient service tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-PAT-01/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-PAT-02 | Synthetic invalid patient fields. Ran `./build/patient_service_test`. Reviewed validation demonstration. | Invalid input is rejected with the offending field. | Executable printed `All patient service tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-PAT-02/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-PAT-03 | Synthetic duplicate patient fixtures. Ran `./build/patient_service_test`. Reviewed duplicate-warning demonstration. | Duplicate patient is detected before save. | Executable printed `All patient service tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-PAT-03/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-PAT-04 | Synthetic IDs, names, and phone values. Ran `./build/patient_service_test`. Reviewed search demonstration. | Matching patient records are returned. | Executable printed `All patient service tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-PAT-04/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-PAT-05 | Synthetic patient with prior demographics. Ran `./build/patient_service_test`. Reviewed update/history demonstration. | Updated values save and history is retained. | Executable printed `All patient service tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-PAT-05/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-APT-01 | Synthetic patient, doctor, future date, and free slot. Ran `./build/appointments_test`. Reviewed booking demonstration. | Available slot is booked successfully. | Executable printed `All appointment tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-APT-01/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-APT-02 | Synthetic occupied appointment slot. Ran `./build/appointments_test`. Reviewed conflict demonstration. | Duplicate booking is rejected and alternatives are shown. | Executable printed `All appointment tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-APT-02/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-APT-03 | Synthetic boundary dates. Ran `./build/appointments_test`. Reviewed date-validation demonstration. | Invalid dates are rejected and valid-window dates accepted. | Executable printed `All appointment tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-APT-03/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-APT-04 | Synthetic appointment eligible for change. Ran `./build/change_appointment_test`. Reviewed cancellation/rescheduling demonstration. | Cancellation/rescheduling updates the appointment and slot. | Executable printed `All change_appointment tests passed`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-APT-04/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-CLN-01 | Synthetic patient, appointment, symptoms, and diagnosis. Ran `./build/clinical_service_test`. Reviewed visit demonstration. | Visit record saves with clinical values. | Executable printed `All clinical service tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-CLN-01/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-CLN-02 | Synthetic visit and prescription fixtures. Ran `./build/clinical_service_test`. Reviewed prescription demonstration. | Prescription saves against the visit. | Executable printed `All clinical service tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-CLN-02/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-CLN-03 | Synthetic allergy, medicine, and override-reason fixtures. Ran `./build/clinical_service_test`. Reviewed allergy demonstration. | Allergy warning appears and override reason is required. | Executable printed `All clinical service tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-CLN-03/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-CLN-04 | Synthetic patient with dated clinical records. Ran `./build/clinical_test`. Reviewed history demonstration. | History is newest first. | Executable printed `All clinical tests passed.`; manual demonstration passed. | [`run.log`](../../evidence/DAKSHA/TC-CLN-04/run.log), [`ctest_verbose.log`](../../evidence/automated_tests/ctest_verbose.log), Sprint videos | **Pass** |
| TC-PERF-01 | Required 100,000 synthetic patients, 100 concurrent users, and 10-minute search run were unavailable. No load command was run. | p95 search response <=2 seconds. | Not measured. | `tests/performance/`; no current run log | **Deferred** |
| TC-PERF-02 | Required isolated appointment dataset, 100 concurrent users, and 10-minute booking run were unavailable. No load command was run. | p95 <3 seconds and no duplicate bookings. | Not measured. | `scripts/appointment_booking_load_test.py`; no current run log | **Deferred** |
| TC-PERF-03 | Current 200-session, 30-minute rerun was not performed. | Error rate below 1% for the 30-minute workload. | Not measured for commit `bbb6006`; historical result is separate evidence. | `tests/performance/NF-003-summary.txt`; no current run log | **Deferred** |

## Summary

| Measure | Count |
|---|---:|
| Assigned cases | 20 |
| Passed | 17 |
| Failed | 0 |
| Deferred | 3 |
| Pass rate among executed cases | 100% (17/17) |

The three performance cases remain deferred and are not counted as passed.
Therefore, the full 20-case execution exit criterion is not demonstrated.

## Maintenance and traceability

The SRS Appendix C mappings are recorded in
[`requirements-status.md`](../requirements-status.md). Maintain the build,
verbose CTest output, per-case logs, CI links, and any future performance
results whenever the implementation or test plan changes.
