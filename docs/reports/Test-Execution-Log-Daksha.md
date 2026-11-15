# Test Execution Log — Daksha SG

## Run metadata

| Field | Observed value |
|---|---|
| Tester | Codex-assisted execution at Daksha SG’s request |
| Execution window | 2026-11-15 22:54–22:59 Asia/Kolkata; exact time per case is in its status record |
| Commit under test | `f88ab6dc5b89d02ef4ab038b87dc9e0f909a3c81` |
| OS | macOS Darwin 25.1.0, arm64 |
| Build | `cmake --build build --parallel 4` — completed; [build output](../../evidence/automated_tests/build.log) |
| Automated tests | `ctest --test-dir build --output-on-failure -V` — 16/16 suites passed in 4.87 seconds; [verbose output](../../evidence/automated_tests/ctest_verbose.log) |
| Live API environment | HMS HTTPS server at loopback with disposable PostgreSQL 15.18; synthetic records only |

The 17 functional and security cases below were exercised through actual HTTPS API requests against the running build. Each case record contains the observed expected/actual result, and its `run.log` contains the sanitized request path/body, HTTP status, response, and named case check. These are API checks, not GUI interactions. Sprint videos may support the user-reported earlier manual work, but were not used to claim current-build execution. No passwords, bearer tokens, or real patient data are included. The captured run date is November 15, 2026; it is not represented as a November 2–6 execution.

## Assigned cases

| Case | Test | Observed result | Evidence | Outcome |
|---|---|---|---|---|
| TC-AUTH-01 | Valid administrator login and liveness | CHECK PASS: valid admin login returned HTTP 200 with role Administrator; returned bearer token was redacted. | [status](../../evidence/DAKSHA/TC-AUTH-01/status.txt), [raw run](../../evidence/DAKSHA/TC-AUTH-01/run.log) | Pass |
| TC-AUTH-02 | Invalid credentials | CHECK PASS: HTTP 401; error code=INVALID_CREDENTIALS | [status](../../evidence/DAKSHA/TC-AUTH-02/status.txt), [raw run](../../evidence/DAKSHA/TC-AUTH-02/run.log) | Pass |
| TC-AUTH-03 | Lockout after repeated failures | CHECK PASS: login HTTP sequence=[401, 401, 401, 401, 423, 423]; expected first four 401 then 423 and locked follow-up 423 | [status](../../evidence/DAKSHA/TC-AUTH-03/status.txt), [raw run](../../evidence/DAKSHA/TC-AUTH-03/run.log) | Pass |
| TC-RBAC-01 | Role-based access control | CHECK PASS: Administrator can list users; Receptionist, Doctor, Billing Officer, and Patient cannot; patient can read own record but cannot read another synthetic patient record. | [status](../../evidence/DAKSHA/TC-RBAC-01/status.txt), [raw run](../../evidence/DAKSHA/TC-RBAC-01/run.log) | Pass |
| TC-PAT-01 | Patient registration | CHECK PASS: HTTP 201; generated patientId=PID-00000003 | [status](../../evidence/DAKSHA/TC-PAT-01/status.txt), [raw run](../../evidence/DAKSHA/TC-PAT-01/run.log) | Pass |
| TC-PAT-02 | Patient input validation | CHECK PASS: HTTP 400; error code=INVALID_PATIENT; response={'error': {'code': 'INVALID_PATIENT', 'message': 'dateOfBirth: Date of birth must be a valid DD-MM-YYYY date.', 'requestId': 'req-21'}} | [status](../../evidence/DAKSHA/TC-PAT-02/status.txt), [raw run](../../evidence/DAKSHA/TC-PAT-02/run.log) | Pass |
| TC-PAT-03 | Duplicate patient detection | CHECK PASS: HTTP 409; error code=DUPLICATE_PATIENT | [status](../../evidence/DAKSHA/TC-PAT-03/status.txt), [raw run](../../evidence/DAKSHA/TC-PAT-03/run.log) | Pass |
| TC-PAT-04 | Patient search | CHECK PASS: HTTP 200; matched ID=True; response={'matches': [{'patientId': 'PID-00000003', 'name': 'Synthetic Patient Alpha Final', 'dateOfBirth': '01-01-1990', 'gender': 'Female', 'phone': '5550101004', 'address': '1 Test Avenue', 'allergies': 'SyntheticPenicillin'}], 'message': ''} | [status](../../evidence/DAKSHA/TC-PAT-04/status.txt), [raw run](../../evidence/DAKSHA/TC-PAT-04/run.log) | Pass |
| TC-PAT-05 | Patient update and history | CHECK PASS: update HTTP 200; history HTTP 200; prior names=['Synthetic Patient Alpha Final', 'Synthetic Patient Alpha Updated', 'Synthetic Patient Alpha'] | [status](../../evidence/DAKSHA/TC-PAT-05/status.txt), [raw run](../../evidence/DAKSHA/TC-PAT-05/run.log) | Pass |
| TC-APT-01 | Available slot booking | CHECK PASS: free slots HTTP 200, count=13, selected=09:30; booking HTTP 201; response={'appointment': {'appointmentId': 6, 'patientId': 'PID-00000003', 'doctorId': 2, 'slotStart': '2026-11-18 09:30', 'status': 'Confirmed'}} | [status](../../evidence/DAKSHA/TC-APT-01/status.txt), [raw run](../../evidence/DAKSHA/TC-APT-01/run.log) | Pass |
| TC-APT-02 | Conflicting appointment slot | CHECK PASS: HTTP 409; code=SLOT_TAKEN; response={'error': {'code': 'SLOT_TAKEN', 'message': 'This slot is already booked.', 'requestId': 'req-28'}, 'suggestedSlots': ['2026-11-18 11:00', '2026-11-18 11:30', '2026-11-18 12:00']} | [status](../../evidence/DAKSHA/TC-APT-02/status.txt), [raw run](../../evidence/DAKSHA/TC-APT-02/run.log) | Pass |
| TC-APT-03 | Appointment date validation | CHECK PASS: past date HTTP 400 code=INVALID_DATE; far date HTTP 400 code=INVALID_DATE | [status](../../evidence/DAKSHA/TC-APT-03/status.txt), [raw run](../../evidence/DAKSHA/TC-APT-03/run.log) | Pass |
| TC-APT-04 | Appointment reschedule and cancellation | CHECK PASS: create HTTP 201, reschedule HTTP 200, cancel HTTP 200, rescheduled={'appointmentId': 3, 'patientId': 'PID-00000003', 'doctorId': 2, 'slotStart': '2026-11-18 13:00', 'status': 'Confirmed'}, cancel response={'appointment': {'appointmentId': 3, 'patientId': 'PID-00000003', 'doctorId': 2, 'slotStart': '2026-11-18 13:00', 'status': 'Cancelled'}} | [status](../../evidence/DAKSHA/TC-APT-04/status.txt), [raw run](../../evidence/DAKSHA/TC-APT-04/run.log) | Pass |
| TC-CLN-01 | Clinical visit creation | CHECK PASS: HTTP 201; visitId=1; response={'visitId': '1'} | [status](../../evidence/DAKSHA/TC-CLN-01/status.txt), [raw run](../../evidence/DAKSHA/TC-CLN-01/run.log) | Pass |
| TC-CLN-02 | Prescription creation | CHECK PASS: HTTP 201; response={'created': True} | [status](../../evidence/DAKSHA/TC-CLN-02/status.txt), [raw run](../../evidence/DAKSHA/TC-CLN-02/run.log) | Pass |
| TC-CLN-03 | Allergy warning and override | CHECK PASS: allergy update 200, appt 201, visit 201, matching medicine HTTP 409 code=ALLERGY_WARNING, override HTTP 201 | [status](../../evidence/DAKSHA/TC-CLN-03/status.txt), [raw run](../../evidence/DAKSHA/TC-CLN-03/run.log) | Pass |
| TC-CLN-04 | Visit history order | CHECK PASS: HTTP 200; diagnosis order=['Synthetic diagnosis B', 'Synthetic diagnosis A']; response={'visits': [{'visitId': '2', 'visitedAt': '2026-11-15 22:54', 'symptoms': 'Synthetic symptom B', 'diagnosis': 'Synthetic diagnosis B', 'notes': 'Synthetic note B', 'prescriptions': [{'item': 'SyntheticPenicillin', 'dosage': '1 tablet', 'frequency': 'daily', 'days': 5, 'overrideReason': 'Synthetic clinical override test'}]}, {'visitId': '1', 'visitedAt': '2026-11-15 22:54', 'symptoms': 'Synthetic symptom A', 'diagnosis': 'Synthetic diagnosis A', 'notes': 'Synthetic note A', 'prescriptions': [{'item': 'Synthetic Medication A', 'dosage': '1 tablet', 'frequency': 'daily', 'days': 5, 'overrideReason': ''}]}]} | [status](../../evidence/DAKSHA/TC-CLN-04/status.txt), [raw run](../../evidence/DAKSHA/TC-CLN-04/run.log) | Pass |
| TC-PERF-01 | Patient search load | Not measured; required 100k patients/100 users/10 minutes not run | [status](../../evidence/DAKSHA/TC-PERF-01/status.txt) | Deferred |
| TC-PERF-02 | Appointment booking load | Not measured; specified concurrent workload not run | [status](../../evidence/DAKSHA/TC-PERF-02/status.txt) | Deferred |
| TC-PERF-03 | Concurrent-session load | Not measured; specified 200-session/30-minute workload not run | [status](../../evidence/DAKSHA/TC-PERF-03/status.txt) | Deferred |

## Summary

| Outcome | Count |
|---|---:|
| Pass | 17 |
| Fail / Partial / Blocked | 0 |
| Deferred | 3 |

The records preserve the actual run date, HTTP results, and test data. No commit or push is included in this update.
