# HMS Test Execution Log — Daksha SG

| Field | Value |
|---|---|
| Project | Hospital Management System (HMS) |
| Tester | Daksha SG |
| Test-case source | HMS Software Test Plan, Appendix A |
| Execution date | 2026-11-15 |
| Build/version | Commit `9dc32b1edffeeb786090540b8c0d84d6e6ad7272`; local CMake build |
| Environment | macOS; existing local build directory; 16 CTest targets |

## Scope and evidence rules

This is the shared execution record for the 20 cases assigned to Daksha SG.
The local run below is automated component/service evidence. It is not claimed
as a substitute for manual UI, end-to-end, load, usability, availability, or
recovery acceptance execution.

The command executed was:

```text
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Actual output: **16/16 tests passed; total test time 12.84 seconds**. The
registered test names are listed in the Evidence column. The source tests and
the command output are repository evidence; the project-level CI evidence is
the [application CI run](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410)
and the [coverage CI run](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994).

`PASS — automated component evidence` means the mapped automated assertions
passed in this run. It does not assert that every Test Plan acceptance step was
performed manually. No individual manual screenshot, console transcript, or
API request/response capture was supplied for these cases.

## Per-case execution records

| Case ID | Tester | Date | Build/version and environment | Result | Brief actual outcome | Evidence |
|---|---|---|---|---|---|---|
| TC-AUTH-01 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Authentication and password verification assertions passed. | `auth_service_test`, `api_test`; `tests/auth_service_test.cpp`, `tests/api_test.cpp`; command/output above |
| TC-AUTH-02 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Invalid-credential validation and generic error assertions passed. | `auth_service_test`, `api_test`; `tests/auth_service_test.cpp`, `tests/api_test.cpp`; command/output above |
| TC-AUTH-03 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Failed-attempt and account-lockout assertions passed. | `auth_service_test`; `tests/auth_service_test.cpp`; command/output above |
| TC-RBAC-01 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Role and permission assertions passed; this is not a complete running-server role matrix. | `auth_service_test`, `api_test`; `tests/auth_service_test.cpp`, `tests/api_test.cpp`; command/output above |
| TC-PAT-01 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Patient registration service assertions passed. | `patient_service_test`; `tests/patient_service_test.cpp`; command/output above |
| TC-PAT-02 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Invalid patient data and field-validation assertions passed. | `patient_service_test`; `tests/patient_service_test.cpp`; command/output above |
| TC-PAT-03 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Duplicate-patient detection assertions passed. | `patient_service_test`; `tests/patient_service_test.cpp`; command/output above |
| TC-PAT-04 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Patient search assertions passed. | `patient_service_test`; `tests/patient_service_test.cpp`; command/output above |
| TC-PAT-05 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Patient update/history assertions passed. | `patient_service_test`; `tests/patient_service_test.cpp`; command/output above |
| TC-APT-01 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Appointment creation and successful booking assertions passed. | `appointments_test`; `tests/appointments_test.cpp`; command/output above |
| TC-APT-02 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Duplicate-slot rejection and next-slot assertions passed. | `appointments_test`; `tests/appointments_test.cpp`; command/output above |
| TC-APT-03 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Booking-window validation assertions passed. | `appointments_test`; `tests/appointments_test.cpp`; command/output above |
| TC-APT-04 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Appointment cancellation/rescheduling assertions passed. | `change_appointment_test`; `tests/change_appointment_test.cpp`; command/output above |
| TC-CLN-01 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Visit and clinical-record assertions passed. | `clinical_service_test`, `clinical_test`; `tests/clinical_service_test.cpp`, `tests/clinical_test.cpp`; command/output above |
| TC-CLN-02 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Prescription assertions passed. | `clinical_service_test`, `clinical_test`; `tests/clinical_service_test.cpp`, `tests/clinical_test.cpp`; command/output above |
| TC-CLN-03 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Allergy warning/override assertions passed. | `clinical_service_test`; `tests/clinical_service_test.cpp`; command/output above |
| TC-CLN-04 | Daksha SG | 2026-11-15 | `9dc32b1`; macOS; CMake/CTest | PASS — automated component evidence | Clinical-history assertions passed. | `clinical_service_test`, `clinical_test`; `tests/clinical_service_test.cpp`, `tests/clinical_test.cpp`; command/output above |
| TC-PERF-01 | Daksha SG | 2026-11-15 | Not executed; acceptance environment unavailable | DEFERRED | No 100,000-patient dataset or 100-user, 10-minute load environment was available; p95 <=2 seconds was not measured. | `tests/performance/`; Test Plan target; no current result artifact |
| TC-PERF-02 | Daksha SG | 2026-11-15 | Not executed; dedicated load environment unavailable | DEFERRED | No 100-user, 10-minute appointment workload or isolated appointment dataset was available; p95 <3 seconds and duplicate-booking criteria were not measured. | `scripts/appointment_booking_load_test.py`; Test Plan target; no current result artifact |
| TC-PERF-03 | Daksha SG | 2026-11-15 | Current rerun not executed; sustained-load environment unavailable | DEFERRED | A current 200-session, 30-minute rerun was not possible. The historical summary is retained as historical evidence only and is not a result for this build. | `tests/performance/NF-003-summary.txt`; current 30-minute run required |

## Final summary

| Measure | Count |
|---|---:|
| Assigned cases | 20 |
| Executed with recorded automated evidence | 17 |
| Passed automated evidence | 17 |
| Failed | 0 recorded |
| Deferred/blocked | 3 |
| Pass rate of executed automated evidence | 100% (17/17) |

The 100% figure is limited to the 17 executed automated component-evidence
records. It is not a 100% pass rate for the 20 Test Plan acceptance cases.
The three deferred cases require extended resources or duration: a 100,000
patient dataset and 100 concurrent users for 10 minutes (TC-PERF-01), a
100-user appointment workload for 10 minutes (TC-PERF-02), and a sustained
200-session run for 30 minutes (TC-PERF-03). Deferral means the Test Plan
full-execution exit criterion is **not demonstrated**.

### Open defects by severity

No defect register or per-case defect records were supplied with this run.
Accordingly, open-defect counts are **not established**, not zero.

| Severity | Open defects |
|---|---|
| Critical | Not established |
| High | Not established |
| Medium | Not established |
| Low | Not established |

### Requirement coverage

The SRS PDF in `docs/Deliverables-Part1/` contains Appendix C, the requirement
traceability matrix. The 20 assigned case IDs map as follows:

| Cases | SRS requirement(s) |
|---|---|
| TC-AUTH-01–03 | HMS-F-001, HMS-F-004, HMS-F-002 |
| TC-RBAC-01 | HMS-F-003 |
| TC-PAT-01–05 | HMS-F-010–014 |
| TC-APT-01–04 | HMS-F-020–023 |
| TC-CLN-01–04 | HMS-F-030–033 |
| TC-PERF-01–03 | HMS-NF-001–003 |

The mappings were checked against the SRS Appendix C and are also reflected
in `docs/requirements-status.md` and `docs/test-cases-updated.md`. The mapping
is present, but deferred cases and the lack of manual acceptance evidence mean
that requirement coverage by a **passed acceptance case** is not demonstrated
for every requirement.

## Maintenance plan

| Activity | Trigger/frequency | Owner | Evidence to retain |
|---|---|---|---|
| Run build, unit, integration, and coverage checks | Every pull request and release | Developer/CI maintainer | CI URL, commit SHA, test and coverage output |
| Review authentication, RBAC, TLS, audit, and session behavior | Every security-sensitive change and release | Backend maintainer/security reviewer | Test log and reviewed change |
| Apply dependency and security updates | Before releases and on advisories | Maintainer | Dependency versions and validation results |
| Verify encrypted backup and restoration | Per hospital backup schedule and before handover | Database administrator | Backup/restore result, elapsed time, RPO/RTO |
| Monitor health and availability | During operation and before releases | Operations maintainer | Health and availability records |
| Re-run performance and usability acceptance | Before major releases or infrastructure changes | QA/performance owner | Dataset, environment, raw load/usability results |
| Update requirements, test cases, and traceability | Whenever requirements, APIs, or architecture change | Requirement owner/QA lead | Revised SRS RTM, Test Plan, execution log |

## Sign-off limitation

This record supplies auditable automated execution evidence and explicitly
records the three deferred cases. It does not claim completion of manual
acceptance steps, and no commit is made as part of this update.
