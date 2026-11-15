# HMS Test Execution Log — Daksha SG

| Field | Value |
|---|---|
| Project | Hospital Management System |
| Tester | Daksha SG |
| Test-case source | HMS Software Test Plan, Appendix A |
| Recorded execution date/time | 2026-11-05 10:00 IST |
| Build/version | Commit `9dc32b1edffeeb786090540b8c0d84d6e6ad7272` |
| Operating system/environment | macOS; CMake build; CTest; 16 registered targets |
| Test data | Synthetic fixtures created by the automated tests; no real patient data |

## Shared automated evidence

The automated execution used this command:

```text
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Recorded runner output:

```text
Test project /Users/dakshasg/SE-Miniproject/build
1/16  backup_service_test .............. Passed
2/16  api_test ......................... Passed
3/16  auth_service_test ................ Passed
4/16  health_test ...................... Passed
5/16  password_security_test ........... Passed
6/16  appointments_test ................ Passed
7/16  change_appointment_test .......... Passed
8/16  doctor_schedule_test ............. Passed
9/16  clinical_service_test ............ Passed
10/16 report_service_test .............. Passed
11/16 billing_service_test ............. Passed
12/16 billing_tax_boundary_test ........ Passed
13/16 receipt_test ..................... Passed
14/16 patient_service_test ............. Passed
15/16 patient_field_encryption_test .... Passed
16/16 clinical_test .................... Passed

100% tests passed out of 16
Total Test time (real) = 12.84 sec
```

The automated output is supplemented by manual demonstration evidence in the
Sprint 1 and Sprint 2 videos listed in `docs/Sprint_Video_links.txt`. The
project-level CI references are the
[application CI run](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410)
and [coverage CI run](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994).

Per-case evidence records are stored in the
[Daksha evidence folder](../../evidence/DAKSHA/README.md). Each case folder
contains a `status.txt` record; the 17 functional/security records point to
the shared raw CTest output and the three performance records explain their
deferral.

## Per-case execution records

### Authentication and authorization

| ID / tester / date-time / build / environment | Setup | Steps performed | Expected and actual result | Evidence | Outcome |
|---|---|---|---|---|---|
| **TC-AUTH-01**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic enabled user and synthetic password fixture. No credentials are recorded. | Ran `ctest --test-dir build --output-on-failure`; executed `auth_service_test` and `api_test`; reviewed the manual demonstration. | Expected valid authentication and password verification to succeed. Actual automated assertions and manual demonstration passed. | Raw output above; test names `auth_service_test`, `api_test`; `tests/auth_service_test.cpp`, `tests/api_test.cpp`; Sprint 1/Sprint 2 videos in `docs/Sprint_Video_links.txt`. | **Pass** |
| **TC-AUTH-02**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic valid and invalid credential fixtures. | Ran the same CTest command; executed invalid-credential assertions in `auth_service_test` and `api_test`; reviewed the manual demonstration. | Expected invalid credentials to return the generic error behavior without revealing which credential failed. Actual automated assertions and manual demonstration passed. | Raw output above; `auth_service_test`, `api_test`; Sprint 1/Sprint 2 videos in `docs/Sprint_Video_links.txt`. | **Pass** |
| **TC-AUTH-03**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic account and five failed-attempt fixture. | Ran the CTest command; executed lockout assertions in `auth_service_test`; reviewed the manual demonstration. | Expected the lockout threshold and locked-account behavior. Actual automated assertions and manual demonstration passed. | Raw output above; `auth_service_test`; Sprint 1/Sprint 2 videos in `docs/Sprint_Video_links.txt`. | **Pass** |
| **TC-RBAC-01**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic Administrator, Doctor, Receptionist, and Patient role fixtures; no passwords recorded. | Ran the CTest command; executed permission assertions in `auth_service_test` and API authorization helper assertions in `api_test`; reviewed the manual demonstration. | Expected permitted operations to succeed and unauthorized operations to be rejected. Actual automated assertions and manual demonstration passed. | Raw output above; `auth_service_test`, `api_test`; Sprint 1/Sprint 2 videos in `docs/Sprint_Video_links.txt`. | **Pass** |

### Patient management

The five patient cases used synthetic patient fixtures and `patient_service_test`.
The registration, validation, duplicate-warning, search, update, and history
checks passed in the automated run and the manual demonstration. Evidence:
the shared [CTest output](../../evidence/automated_tests/daksha_ctest_20261105.log)
and the Sprint 1/Sprint 2 videos listed in `docs/Sprint_Video_links.txt`.

| Case | Expected and actual result | Outcome |
|---|---|---|
| TC-PAT-01 | Registration succeeded in automated and manual checks. | **Pass** |
| TC-PAT-02 | Invalid input validation succeeded in automated and manual checks. | **Pass** |
| TC-PAT-03 | Duplicate detection succeeded in automated and manual checks. | **Pass** |
| TC-PAT-04 | Patient search succeeded in automated and manual checks. | **Pass** |
| TC-PAT-05 | Patient update/history succeeded in automated and manual checks. | **Pass** |

### Appointments

The four appointment cases used synthetic patient, doctor, date, and slot
fixtures. Booking, duplicate-slot handling, booking-window validation, and
cancellation/rescheduling passed in the automated run and manual demonstration.

| Case | Expected and actual result | Outcome |
|---|---|---|
| TC-APT-01 | Available-slot booking succeeded in automated and manual checks. | **Pass** |
| TC-APT-02 | Duplicate-slot rejection and alternatives succeeded in automated and manual checks. | **Pass** |
| TC-APT-03 | Booking-window validation succeeded in automated and manual checks. | **Pass** |
| TC-APT-04 | Cancellation/rescheduling succeeded in automated and manual checks. | **Pass** |

### Clinical records

The four clinical cases used synthetic patients, appointments, visit values,
prescriptions, allergies, and dated history records. Visit recording,
prescriptions, allergy override, and history ordering passed in the automated
run and manual demonstration.

| Case | Expected and actual result | Outcome |
|---|---|---|
| TC-CLN-01 | Visit recording succeeded in automated and manual checks. | **Pass** |
| TC-CLN-02 | Prescription handling succeeded in automated and manual checks. | **Pass** |
| TC-CLN-03 | Allergy warning/override succeeded in automated and manual checks. | **Pass** |
| TC-CLN-04 | Clinical-history ordering succeeded in automated and manual checks. | **Pass** |

### Performance cases

| ID / tester / date-time / build / environment | Setup | Steps performed | Expected and actual result | Evidence | Outcome |
|---|---|---|---|---|---|
| **TC-PERF-01**<br>Daksha SG<br>2026-11-05 10:00 IST | Required setup: 100,000 synthetic patients, 100 concurrent users, 10-minute search workload. | No load command was run because the required dataset and load environment were unavailable. | Expected p95 search response time <=2 seconds. Actual: not measured. | `tests/performance/`; no current load output or CI artifact. | **Deferred** — resource unavailable |
| **TC-PERF-02**<br>Daksha SG<br>2026-11-05 10:00 IST | Required setup: isolated synthetic appointment data, 100 concurrent users, 10-minute booking workload. | No load command was run because the dedicated environment and dataset were unavailable. | Expected p95 <3 seconds and no duplicate bookings. Actual: not measured. | `scripts/appointment_booking_load_test.py`; no current load output. | **Deferred** — resource unavailable |
| **TC-PERF-03**<br>Daksha SG<br>2026-11-05 10:00 IST | Required setup: sustained 200-session workload for 30 minutes. | No current rerun was performed. | Expected 200 concurrent sessions for 30 minutes with error rate below 1%. Actual: not measured for this build; historical summary is not a current execution. | `tests/performance/NF-003-summary.txt`; no current 30-minute runner output. | **Deferred** — duration/environment unavailable |

## Final summary

| Measure | Count |
|---|---:|
| Assigned cases | 20 |
| Cases with automated execution evidence | 17 |
| Passed | 17 |
| Partial | 0 |
| Failed | 0 recorded |
| Deferred/blocked | 3 |
| Pass rate | 100% of executed cases (17/17); the three performance cases remain deferred |

The 17 passing records combine automated component assertions with the manual
demonstrations. The three deferred cases
require extended resources or duration, so the Test Plan full-execution exit
criterion is not demonstrated.

## Defects by severity

No defect register or per-case defect records were supplied. Counts are
therefore **not established**, not zero.

| Severity | Open defects |
|---|---|
| Critical | Not established |
| High | Not established |
| Medium | Not established |
| Low | Not established |

## Requirement coverage and maintenance

The SRS PDF under `docs/Deliverables-Part1/` contains Appendix C, the
requirement traceability matrix. The assigned IDs map to HMS-F-001–004,
HMS-F-010–014, HMS-F-020–023, HMS-F-030–033, and HMS-NF-001–003 as recorded
in the SRS and `docs/requirements-status.md`. The mappings exist, but
deferred cases and partial automated-only evidence mean passed acceptance
coverage is not demonstrated for every mapped requirement.

| Maintenance activity | Trigger/frequency | Evidence to retain |
|---|---|---|
| Run build, unit, integration, and coverage checks | Every pull request and release | CI URL, commit SHA, raw output |
| Review authentication, RBAC, TLS, audit, and session behavior | Security-sensitive change and each release | Test log and reviewed change |
| Verify encrypted backup and restoration | Hospital backup schedule and before handover | Timed backup/restore result, RPO/RTO |
| Re-run performance and usability acceptance | Major release or infrastructure change | Dataset, environment, raw results |
| Update requirements, test cases, and traceability | Requirement/API/architecture change | Revised SRS RTM, Test Plan, execution log |

## Sign-off limitation

This record uses the Sprint 1 and Sprint 2 demonstration videos as manual
evidence for the 17 functional/security cases. The three performance cases
remain deferred and are not counted as passed. No commit was created for this
update.
