**# HMS Test Execution Log — Daksha SG**


**Project:\*\*** Hospital Management System (HMS)  

**Tester:\*\*** Daksha SG  

**Test-case source:\*\*** HMS Software Test Plan, Appendix A  

**Repository:\*\*** https\://github.com/hms-software-engineering/SE-Miniproject  


**## Purpose**



This file records the 20 test cases assigned to Daksha SG. It distinguishes project-owner-reported completion from the performance cases deferred for the current submission. It does not claim that an individual execution artifact exists where none was supplied.



**## Evidence and recording notes**



The supplied project verification-status review states that the project owner confirmed **34 of 40 Test Plan cases completed through manual and automated testing\*\***. It identifies six deferred cases, including the three performance cases assigned to Daksha. The 17 assigned functional/security cases are not listed among those six deferred cases; they are therefore recorded below as **Completed — owner-confirmed\*\***, not as independently evidenced individual executions.



The supplied review does not provide an individual execution timestamp, tester console transcript, screenshot, or request/response capture for every functional/security case. Do not interpret the shared CI evidence as proof that every manual acceptance step was individually performed by Daksha.



**### Common supporting evidence**



\- **Application CI:\*\*** [GitHub Actions run 43632305410](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410), commit `ed4af4a214721c2e7e8f07d3162be65b803385b6`. The review reports successful Ubuntu and Windows builds and 16 registered CTest suites passing.

\- **Coverage CI:\*\*** [GitHub Actions run 43625440994](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994), commit `b170901`. The review reports 16 registered CTest suites passing in the PostgreSQL-enabled coverage job.

\- **Local reruns:\*\*** The supplied review records all 16 registered CTest suites passing on Windows and WSL on 15 November 2026.

\- **Project reports:\*\*** `docs/reports/Unit-Test-Report-2026-11-14.md`, `docs/reports/System-Validation-Report-2026-11-14.md`, `docs/reports/Code-Coverage-Report-2026-11-14.md`, and `docs/reports/Branch-Coverage-Report-2026-11-14.md`.



**## Assigned test cases**



| Case ID | Tester | Date | Build / version | Status | Brief actual result / execution note | Evidence |

|---|---|---|---|---|---|---|

| TC-AUTH-01 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Authentication implementation and related automated tests are present; the registered CTest suites passed in the reported CI run. | `tests/auth_service_test.cpp`, `tests/api_test.cpp`; [CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410). Individual login execution output was not supplied. |

| TC-AUTH-02 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Generic login-error behavior is implemented and covered by authentication/API tests; registered CTest suites passed. | `tests/auth_service_test.cpp`, `tests/api_test.cpp`; `docs/reports/Unit-Test-Report-2026-11-14.md`. Individual invalid-credential output was not supplied. |

| TC-AUTH-03 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Account-lockout behavior is implemented in the authentication service and related tests passed in CI. | `tests/auth_service_test.cpp`; [CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410). Individual lockout execution output was not supplied. |

| TC-RBAC-01 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Server-side role/permission checks are implemented and covered by authentication/API tests. A separate read-only server check reportedly confirmed unauthenticated user listing is rejected. | `tests/auth_service_test.cpp`, `tests/api_test.cpp`; `docs/reports/System-Validation-Report-2026-11-14.md`. Individual role-by-role output was not supplied. |

| TC-PAT-01 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Patient-registration functionality and related patient-service tests are present; registered CTest suites passed. | Patient-service tests under `tests/`; `docs/reports/Unit-Test-Report-2026-11-14.md`. Individual registration request/response was not supplied. |

| TC-PAT-02 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Patient input validation is implemented and covered by patient-service tests; registered CTest suites passed. | Patient-service tests under `tests/`; [CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410). Individual invalid-input output was not supplied. |

| TC-PAT-03 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Duplicate-patient detection/warning behavior is implemented and covered by patient-service tests. | Patient-service tests under `tests/`; `docs/reports/Unit-Test-Report-2026-11-14.md`. Individual duplicate-warning output was not supplied. |

| TC-PAT-04 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Patient-search functionality is implemented and covered by patient-service tests; registered CTest suites passed. | Patient-service tests under `tests/`; [CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410). Individual search output was not supplied. |

| TC-PAT-05 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Patient-update/history functionality is implemented and covered by patient-service tests. | Patient-service tests under `tests/`; `docs/reports/Unit-Test-Report-2026-11-14.md`. Individual update/history output was not supplied. |

| TC-APT-01 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Appointment-booking functionality is implemented and covered by appointment tests; registered CTest suites passed. | `tests/appointments_test.cpp`; [CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410). Individual booking output was not supplied. |

| TC-APT-02 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Appointment slot-conflict/unique-booking behavior is implemented and covered by appointment tests. | `tests/appointments_test.cpp`; `docs/reports/Unit-Test-Report-2026-11-14.md`. Individual successful/conflicting booking output was not supplied. |

| TC-APT-03 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Booking date/window validation is implemented and covered by appointment tests. | `tests/appointments_test.cpp`; [CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410). Individual past-date, elapsed-today, day-60 and day-61 outputs were not supplied. |

| TC-APT-04 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Reschedule/cancel behavior is implemented and covered by change-appointment tests. | `tests/change_appointment_test.cpp`; [CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410). Individual cancellation/rescheduling output was not supplied. |

| TC-CLN-01 | Daksha SG | Not recorded | HMS; coverage commit `b170901` | Completed — owner-confirmed | Clinical visit-recording functionality and related clinical-service tests are present; the PostgreSQL-enabled coverage job passed. | Clinical-service tests under `tests/`; [coverage CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994). Individual visit-creation output was not supplied. |

| TC-CLN-02 | Daksha SG | Not recorded | HMS; coverage commit `b170901` | Completed — owner-confirmed | Prescription functionality is implemented and covered by clinical-service tests; the PostgreSQL-enabled coverage job passed. | Clinical-service tests under `tests/`; [coverage CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994). Individual prescription output was not supplied. |

| TC-CLN-03 | Daksha SG | Not recorded | HMS; CI commit `ed4af4a` | Completed — owner-confirmed | Allergy warning/override behavior, including the required override reason, is implemented and covered by clinical-service tests. | Clinical-service tests under `tests/`; `docs/reports/Unit-Test-Report-2026-11-14.md`. Individual warning/override output was not supplied. |

| TC-CLN-04 | Daksha SG | Not recorded | HMS; coverage commit `b170901` | Completed — owner-confirmed | Clinical-history functionality is implemented and covered by clinical-service tests; the PostgreSQL-enabled coverage job passed. | Clinical-service tests under `tests/`; [coverage CI run](https\://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994). Individual patient-history output was not supplied. |

| TC-PERF-01 | Daksha SG | Not executed for this submission | Not applicable — current acceptance run deferred | Deferred | The current 100,000-patient / 100-concurrent-user patient-search acceptance run was deferred. The supplied verification-status review says the required dataset and load environment were not available. No current acceptance result is claimed. | Performance assets are reported under `tests/performance/`. Test Plan acceptance target: 100,000 patients, 100 virtual users, 10-minute run, p95 search response time ≤2 seconds. No current result file was supplied. |

| TC-PERF-02 | Daksha SG | Not executed for this submission | Not applicable — current acceptance run deferred | Deferred | The current appointment-booking load acceptance run was deferred. The supplied verification-status review says a dedicated load environment and test data were required. No current acceptance result is claimed. | `scripts/appointment_booking_load_test.py`. Test Plan acceptance target: 100 virtual users booking appointments for 10 minutes, p95 response time <3 seconds and no duplicate bookings. No current result file was supplied. |

| TC-PERF-03 | Daksha SG | Historical run date not recorded; current rerun deferred | Historical build/commit not recorded | Deferred — historical result available | The historical summary reports 200 concurrent sessions for 30 minutes, 345,656 requests, 344,237 successful requests, 1,419 failed requests, a 0.411% error rate, average response time of 20 ms, p95 of 39 ms, maximum of 761 ms, and that the server survived. The current rerun was deferred. Do not treat the historical result as a current-build rerun. | `tests/performance/NF-003-summary.txt`. Attach the original JMeter result file if available; the historical run date and build were not recorded in the supplied material. |



**## Final execution summary

### Assigned-case disposition

| Measure | Count | Interpretation |
| --- | ---: | --- |
| Assigned cases | 20 | TC-AUTH-01–03, TC-RBAC-01, TC-PAT-01–05, TC-APT-01–04, TC-CLN-01–04, TC-PERF-01–03 |
| Owner-confirmed completed | 17 | Project-owner confirmation; not independently evidenced per-case executions |
| Independently evidenced individual cases in this log | 0 | The supplied material does not contain per-case execution transcripts, screenshots, or request/response records attributable to this tester |
| Failed individual cases | Not established | No per-case failure record was supplied |
| Deferred for this submission | 3 | TC-PERF-01, TC-PERF-02, and the current rerun of TC-PERF-03 |
| Pass rate for assigned cases | Not calculable | A defensible numerator and denominator of individually executed cases are unavailable; do not treat the 16 passing CTest suites as 16 passed Test Plan cases |

The 17 functional/security cases remain recorded as **Completed — owner-confirmed** in the case table. This is not the same as an independently verified pass for each manual Test Plan case. The 16 CTest suites passed in the reported Ubuntu/Windows CI and PostgreSQL-enabled coverage job, but the System Validation Report explicitly says these suites are not a one-to-one execution of the 40 Appendix A cases.

### Project-wide disposition

The project verification review reports 34 of 40 Test Plan cases completed and six deferred. The six deferred project-wide cases are TC-PERF-01, TC-PERF-02, TC-PERF-03 current rerun, TC-AVL-01, TC-USA-01, and TC-REL-01. Only the first three are assigned to Daksha in this log.

Because six planned cases remain deferred, the Test Plan exit criterion requiring **100% of planned test cases executed** is not demonstrated. The other exit criteria (at least 95% passed, zero open critical/high defects, every requirement covered by at least one passed case, and acceptance criteria satisfied) also cannot be declared satisfied from the supplied per-case evidence.

### Defects

| Severity | Open defects |
| --- | --- |
| Critical | Not established from the supplied records |
| High | Not established from the supplied records |
| Medium | Not established from the supplied records |
| Low | Not established from the supplied records |

No consolidated defect register or verified open-defect-by-severity count was available in the reviewed repository documents. “Not established” must not be interpreted as zero open defects.

### Environment and build evidence

- **Windows CI:** Windows Server 2022 / Visual Studio 2022; build and all 16 registered CTest suites passed in GitHub Actions run [43632305410](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43632305410).
- **Ubuntu CI:** Ubuntu 22.04; build and all 16 registered CTest suites passed in the same run.
- **PostgreSQL-enabled coverage job:** all 16 suites passed; coverage artifact from commit `b170901620c751fd552ba5e1f3a0732be69fab7c`, run [43625440994](https://github.com/hms-software-engineering/SE-Miniproject/actions/runs/43625440994).
- **Local reruns on 15-Nov-2026:** Windows 16/16 CTest suites passed in 12.43 seconds; WSL 16/16 passed in 18.61 seconds. The WSL rerun did not enable PostgreSQL integration flags.
- These are shared project-level build/test results. They are not proof that each assigned manual test case was personally executed by the named tester.

### Requirement traceability (verified against SRS Appendix C)

The SRS already contains **Appendix C: Requirement Traceability Matrix**. The assigned case IDs in this log match the corresponding entries in that matrix:

| Assigned test case | SRS requirement ID | Requirement |
| --- | --- | --- |
| TC-AUTH-01 | HMS-F-001 | Authenticate users with a unique username and password |
| TC-AUTH-02 | HMS-F-004 | Display a generic message for invalid credentials |
| TC-AUTH-03 | HMS-F-002 | Lock an account for 15 minutes after five failed attempts |
| TC-RBAC-01 | HMS-F-003 | Enforce role-based access control |
| TC-PAT-01 | HMS-F-010 | Register a new patient |
| TC-PAT-02 | HMS-F-011 | Reject invalid registration data and identify the offending field |
| TC-PAT-03 | HMS-F-012 | Warn about a duplicate patient before saving |
| TC-PAT-04 | HMS-F-013 | Search patients by ID, name, or phone |
| TC-PAT-05 | HMS-F-014 | Update demographics while retaining previous values |
| TC-APT-01 | HMS-F-020 | Book an appointment |
| TC-APT-02 | HMS-F-021 | Prevent double-booking and show the next three slots |
| TC-APT-03 | HMS-F-022 | Reject past and more-than-60-day bookings |
| TC-APT-04 | HMS-F-023 | Reschedule or cancel up to two hours before the appointment |
| TC-CLN-01 | HMS-F-030 | Record symptoms, diagnosis, and visit notes |
| TC-CLN-02 | HMS-F-031 | Add prescription items to a visit |
| TC-CLN-03 | HMS-F-032 | Warn about allergies and require an override reason |
| TC-CLN-04 | HMS-F-033 | Display patient history in reverse chronological order |
| TC-PERF-01 | HMS-NF-001 | Patient search within two seconds for 95% of requests |
| TC-PERF-02 | HMS-NF-002 | Appointment booking within three seconds for 95% of requests |
| TC-PERF-03 | HMS-NF-003 | Support 200 concurrent sessions for 30 minutes |

**RTM result:** All 20 test-case IDs assigned to Daksha in this log have matching requirement-to-test-case entries in SRS Appendix C. This confirms the mapping exists; it does not mean every mapped requirement has a verified passing execution. The SRS RTM itself does not need to be described as missing or awaiting creation.

## Maintenance plan

The following activities are recommended for maintaining the HMS after handover.

| Maintenance activity | Recommended trigger / frequency | Responsible role | Record to retain |
| --- | --- | --- | --- |
| Review and apply security/dependency updates | Before releases and when security advisories arise | Maintainer | Dependency versions and validation results |
| Review authentication, RBAC, audit, and session-security behavior | Every security-sensitive change and each release | Backend maintainer / security reviewer | Relevant test results and reviewed PR |
| Verify database backups and test restoration | According to the hospital's backup schedule | Database administrator | Backup and restoration results |
| Monitor service health and availability | During operation and before releases | Operations maintainer | Health-check results and downtime records |
| Review logs and audit records | Periodically and after a security incident | Administrator | Audit review and incident reference |
| Run automated build, unit, integration, and coverage checks | On every pull request and release | Developer / CI maintainer | CI run URL, commit SHA, test and coverage reports |
| Re-run performance and usability acceptance tests | Before major releases and when relevant infrastructure changes | QA / performance test owner | Dataset/configuration, environment, raw load/usability results |
| Update documentation and traceability | Whenever a requirement, API, test case, or architecture changes | Requirement owner / QA lead | Updated SRS RTM, Test Plan, test log, and change history |

Maintenance activities should follow the approved SRS, Architecture and Design Specification, Test Plan, and repository operating procedures.

## Requirements traceability follow-up

The repository contains `docs/requirements-status.md` and `docs/test-cases-updated.md`, and the Test Plan states that its Section 13 is the full test-case-to-requirement traceability matrix. The SRS source document itself was not located among the repository files available for this review. Therefore, this log does not claim that the SRS RTM has been updated.

Before final sign-off:
1. Locate the current approved SRS and its RTM.
2. Check every requirement ID against its assigned Test Plan case(s).
3. Confirm that each requirement has at least one **passed** test case, not merely an implemented feature or an automated component test.
4. Mark deferred/unverified requirements clearly and update the RTM revision history.
5. Cross-check the SRS RTM against `docs/requirements-status.md` and `docs/test-cases-updated.md`.
