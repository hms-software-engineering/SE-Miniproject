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

This output is component-test evidence, not proof of manual UI/API acceptance.
No individual screenshots, sanitized request/response captures, or manual
console transcripts were available. The project-level CI references are the
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
| **TC-AUTH-01**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic enabled user and synthetic password fixture. No credentials are recorded. | Ran `ctest --test-dir build --output-on-failure`; executed `auth_service_test` and `api_test`. | Expected valid authentication and password verification to succeed. Actual mapped assertions passed; the manual login UI/API flow was not run. | Raw output above; test names `auth_service_test`, `api_test`; `tests/auth_service_test.cpp`, `tests/api_test.cpp`. | **Partial** — automated evidence only |
| **TC-AUTH-02**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic valid and invalid credential fixtures. | Ran the same CTest command; executed invalid-credential assertions in `auth_service_test` and `api_test`. | Expected invalid credentials to return the generic error behavior without revealing which credential failed. Actual mapped assertions passed; no live HTTP status/body capture was recorded. | Raw output above; `auth_service_test`, `api_test`; source tests. | **Partial** — automated evidence only |
| **TC-AUTH-03**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic account and five failed-attempt fixture. | Ran the CTest command; executed lockout assertions in `auth_service_test`. | Expected the lockout threshold and locked-account behavior. Actual mapped assertions passed; no manual timed lockout observation was recorded. | Raw output above; `auth_service_test`; `tests/auth_service_test.cpp`. | **Partial** — automated evidence only |
| **TC-RBAC-01**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic Administrator, Doctor, Receptionist, and Patient role fixtures; no passwords recorded. | Ran the CTest command; executed permission assertions in `auth_service_test` and API authorization helper assertions in `api_test`. | Expected permitted operations to succeed and unauthorized operations to be rejected. Actual mapped assertions passed; a complete running-server role matrix and HTTP status capture were not performed. | Raw output above; `auth_service_test`, `api_test`; source tests. | **Partial** — automated evidence only |

### Patient management

| ID / tester / date-time / build / environment | Setup | Steps performed | Expected and actual result | Evidence | Outcome |
|---|---|---|---|---|---|
| **TC-PAT-01**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic patient fixture and isolated test repository. | Ran the CTest command; executed `patient_service_test` registration cases. | Expected a valid synthetic patient to be saved and assigned an identifier. Actual registration assertions passed; no manual registration request/response was captured. | Raw output above; `patient_service_test`; `tests/patient_service_test.cpp`. | **Partial** — automated evidence only |
| **TC-PAT-02**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic invalid names, dates, phone values, and required fields. | Ran the CTest command; executed validation cases in `patient_service_test`. | Expected invalid input to be rejected and the offending field identified. Actual mapped assertions passed; no UI/API field-level response was captured. | Raw output above; `patient_service_test`; source test. | **Partial** — automated evidence only |
| **TC-PAT-03**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Two synthetic patient records with duplicate identifying data. | Ran the CTest command; executed duplicate detection cases in `patient_service_test`. | Expected a duplicate warning before save. Actual duplicate-detection assertions passed; no manual warning screenshot or response body was captured. | Raw output above; `patient_service_test`; source test. | **Partial** — automated evidence only |
| **TC-PAT-04**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic patient IDs, names, and phone values. | Ran the CTest command; executed search cases in `patient_service_test`. | Expected matching records for supported search fields. Actual search assertions passed; no live search request or result capture was recorded. | Raw output above; `patient_service_test`; source test. | **Partial** — automated evidence only |
| **TC-PAT-05**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic patient record with prior demographic values. | Ran the CTest command; executed update/history cases in `patient_service_test`. | Expected updated demographics and retained previous values/history. Actual mapped assertions passed; no UI/API history output was captured. | Raw output above; `patient_service_test`; source test. | **Partial** — automated evidence only |

### Appointments

| ID / tester / date-time / build / environment | Setup | Steps performed | Expected and actual result | Evidence | Outcome |
|---|---|---|---|---|---|
| **TC-APT-01**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic patient, doctor, future date, and available slot. | Ran the CTest command; executed successful booking cases in `appointments_test`. | Expected an available slot to be booked successfully. Actual booking assertions passed; no live booking request/confirmation was captured. | Raw output above; `appointments_test`; `tests/appointments_test.cpp`. | **Partial** — automated evidence only |
| **TC-APT-02**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic patient/doctor and an already occupied slot. | Ran the CTest command; executed duplicate-slot cases in `appointments_test`. | Expected duplicate booking rejection and three alternative slots. Actual assertions passed; no HTTP status or response body was captured. | Raw output above; `appointments_test`; source test. | **Partial** — automated evidence only |
| **TC-APT-03**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic dates covering past, current, 60-day, and beyond-60-day boundaries. | Ran the CTest command; executed date-window cases in `appointments_test`. | Expected invalid dates to be rejected and valid-window dates accepted. Actual mapped assertions passed; no live API status/value capture was recorded. | Raw output above; `appointments_test`; source test. | **Partial** — automated evidence only |
| **TC-APT-04**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic appointment eligible for cancellation/rescheduling. | Ran the CTest command; executed change cases in `change_appointment_test`. | Expected allowed cancellation/rescheduling to update status and release or move the slot. Actual assertions passed; no manual confirmation was captured. | Raw output above; `change_appointment_test`; `tests/change_appointment_test.cpp`. | **Partial** — automated evidence only |

### Clinical records

| ID / tester / date-time / build / environment | Setup | Steps performed | Expected and actual result | Evidence | Outcome |
|---|---|---|---|---|---|
| **TC-CLN-01**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic patient, doctor, appointment, symptoms, and diagnosis values. | Ran the CTest command; executed visit-record cases in `clinical_service_test` and `clinical_test`. | Expected a visit record to save with the supplied clinical values. Actual mapped assertions passed; no API response or UI confirmation was captured. | Raw output above; `clinical_service_test`, `clinical_test`; source tests. | **Partial** — automated evidence only |
| **TC-CLN-02**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic visit and prescription item fixtures. | Ran the CTest command; executed prescription cases in `clinical_service_test` and `clinical_test`. | Expected prescription items to save against the visit. Actual mapped assertions passed; no manual/API output was captured. | Raw output above; `clinical_service_test`, `clinical_test`; source tests. | **Partial** — automated evidence only |
| **TC-CLN-03**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic patient allergy and prescription fixtures, including an override reason. | Ran the CTest command; executed allergy warning/override cases in `clinical_service_test`. | Expected an allergy warning and a required override reason. Actual mapped assertions passed; no warning/override response or screenshot was captured. | Raw output above; `clinical_service_test`; `tests/clinical_service_test.cpp`. | **Partial** — automated evidence only |
| **TC-CLN-04**<br>Daksha SG<br>2026-11-05 10:00 IST<br>`9dc32b1`; macOS; CMake/CTest | Synthetic patient with multiple dated clinical records. | Ran the CTest command; executed history ordering cases in `clinical_service_test` and `clinical_test`. | Expected history in reverse chronological order. Actual mapped assertions passed; no UI/API history output was captured. | Raw output above; `clinical_service_test`, `clinical_test`; source tests. | **Partial** — automated evidence only |

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
| Passed | 0 |
| Partial | 17 |
| Failed | 0 recorded |
| Deferred/blocked | 3 |
| Pass rate | Not established; no case is marked Pass because required manual/end-to-end checks lack evidence |

The 17 partial records demonstrate mapped automated component assertions only.
They do not demonstrate full Test Plan acceptance. The three deferred cases
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

This record is intentionally limited to the evidence available. It does not
claim manual execution that was not supported by a screenshot, sanitized
request/response, console capture, or equivalent raw artifact. No commit was
created for this update.
