# HMS Test Cases — Sprint Update

**Revision date:** 2026-11-12
**Baseline:** HMS Test Plan v1.0, Appendix A and its traceability matrix
**Purpose:** Add the sprint-era API, security, encryption, and acceptance details
to the approved 40-case baseline without changing its stable test-case IDs.

The original PDF remains the approved baseline. This Markdown addendum is the
updated working test-case document. It records test-oracle changes and the
actual execution status separately; an automated component test does not
automatically pass the full manual or system-level test case.

## Test-case updates

| Existing case(s) | Updated setup, steps, or expected result |
| --- | --- |
| TC-AUTH-01 | Bootstrap the first Administrator using `HMS_BOOTSTRAP_ADMIN_USERNAME` and `HMS_BOOTSTRAP_ADMIN_PASSWORD`, then authenticate over HTTPS at `POST /api/v1/auth/login`. Verify a bearer token, role, and expiry are returned. The old development seed accounts have deliberately invalid password hashes and are not login test accounts. |
| TC-AUTH-02, TC-AUTH-03 | Use the generic invalid-credentials response for both unknown users and wrong passwords. Verify lockout after five consecutive failures and that the account stays locked for 15 minutes. Confirm no password or token appears in logs. |
| TC-RBAC-01 | Exercise the actual API with each role's bearer token, not only hidden UI controls. Expected denial is HTTP 403 with `Access denied`; check ownership restrictions for Patient records and that protected endpoints reject missing/invalid tokens. |
| TC-PAT-01, TC-PAT-02 | Register using `POST /api/v1/patients` with `name`, `dateOfBirth` (`DD-MM-YYYY`), `gender`, `phone`, `address`, and optional `allergies`. Verify the generated ID matches `PID-nnnnnnnn`, server-side validation names the invalid field, and patient PHI is encrypted at rest. |
| TC-PAT-03 | A matching patient produces HTTP 409 `DUPLICATE_PATIENT`; save only after explicit user confirmation and retry with `?confirmDuplicate=true`. Patient accounts creating their own linked profile use `/api/v1/patients/me/profile` and do not use the receptionist duplicate-confirmation flow. |
| TC-PAT-04, TC-PAT-05 | Search through `GET /api/v1/patients/search?query=...` by ID, exact phone, or case-insensitive name prefix. Verify update history preserves encrypted old values, timestamp, and actor; verify the receptionist UI round-trips the optional allergies field. |
| TC-APT-01, TC-APT-02 | Get slots from `GET /api/v1/appointments/free-slots?doctorId=...&date=...`; book through `POST /api/v1/appointments` with string `patientId`, `doctorId`, `date` (`YYYY-MM-DD`), and `time` (`HH:MM`). Verify atomic database uniqueness and a conflict response with up to three `suggestedSlots`. |
| TC-APT-03, TC-APT-04 | Check past-date/time and 60-day booking boundaries, then cancel/reschedule only when at least two hours remain. Verify conflicts do not create duplicate confirmed bookings and cancellation frees the slot. Use a disposable test database for any destructive setup. |
| TC-APT-05 | Use `GET /api/v1/doctors/me/schedule?date=...`; compare the implementation against the F-024 differences recorded in `docs/deviations.md` (cancelled items, numeric ID, space separator, extra doctorId). Do not silently treat the example response and implementation as identical. |
| TC-CLN-01, TC-CLN-02 | Record a visit using `POST /api/v1/visits` and add a prescription using `POST /api/v1/visits/{visitId}/prescriptions`. Verify the visit belongs to the doctor's confirmed appointment, is stamped with doctor/time, and clinical fields are encrypted at rest. |
| TC-CLN-03 | Set a synthetic patient allergy using `PUT /api/v1/patients/{patientId}/allergies`. Prescribe a matching medicine without `overrideReason` and expect HTTP 409 `ALLERGY_WARNING`; submit with a non-empty reason and verify the prescription and override audit event. Include unreadable/corrupt allergy ciphertext as a fail-safe warning, not as “no allergy”. |
| TC-CLN-04, TC-CLN-05 | Verify doctor history is newest-first with prescriptions. For a Patient account, verify `/api/v1/patients/me/history` and `/api/v1/patients/me/bills` are own-record-only and another patient's history request is denied. These routes currently have no dedicated patient-history/bills desktop screen. |
| TC-BIL-01, TC-BIL-02 | Create an itemized bill with configured `HMS_BILL_TAX_RATE`; record Cash, Card, or UPI payments. Verify cents rounding, Unpaid/Partially Paid/Paid states, overpayment rejection, and one bill per visit. Run only with synthetic data. |
| TC-BIL-03 | Generate a receipt from the implemented `GET /api/v1/payments/{paymentId}/receipt` route. Verify unique receipt number, IST display date, and balance as of that payment. The route and flat payment API differ from the SAD examples; see `docs/deviations.md`. |
| TC-RPT-01 | Request a date-bounded report using `GET /api/v1/reports?startDate=...&endDate=...&format=csv|pdf`. Verify inclusive dates, appointment counts, revenue from payments, attachment headers, and malformed/reversed ranges. |
| TC-ADM-01 | Create a user with `POST /api/v1/users`, deactivate with `PATCH /api/v1/users/{userId}` and `{"action":"deactivate"}`, then verify login and existing-session authentication fail. Verify safeguards against deactivating the acting/last administrator. |
| TC-ADM-02 | Filter administrator audit entries by user/date/action. Verify unauthorized roles receive 403 and response shape `{ "entries": [...] }`, which differs from the SAD's array example (see `docs/deviations.md`). Audit records must not disclose secrets or clinical text. |
| TC-PERF-01 | Seed 100,000 synthetic patient records and 100 concurrent users; search for 10 minutes and record p95 ≤ 2 seconds, errors, database seed/count, host, and run timestamps. Existing JMeter assets are test inputs, not evidence of a current passing run. |
| TC-PERF-02 | Use 100 distinct authorized synthetic booking requests, unique free slots, and a disposable database; record p95 < 3 seconds and all status/error details. Do not reuse slots or tokens from another test run. |
| TC-PERF-03 | Run the mixed workload at 200 concurrent sessions for 30 minutes. A checked-in historical result reports 345,656 requests, 0.411% errors, and survival; re-run for a current acceptance result and preserve raw evidence. |
| TC-AVL-01 | Poll readiness at one-minute intervals for the complete agreed measurement period, retain timestamped successes/failures/missing intervals, and calculate service-window availability. Unit tests for a calculator do not prove calendar-period availability. |
| TC-USA-01 | Follow `docs/usability/NF-005.md` and `docs/usability/NF-009.md`: synthetic data, five participants, 50 attempts, independent completion numerator/denominator, help usage, timing, and blocked tasks. Do not derive participant results from automated tests. |
| TC-REL-01 | On a disposable database, run encrypted backup and restore, validate restored data, and time the full authorized drill. The unit test covers cryptographic handling; the four-hour operational RTO has not been demonstrated by that test. |
| TC-PORT-01 | Build and smoke-test the GUI on Windows 10/11 with MSVC and Ubuntu 22.04 with GCC. The current recorded Windows environment is Windows Server/Visual Studio 2022 CI; MSVC 2019 and interactive desktop acceptance remain separate checks. |
| TC-MNT-01 | Measure executable service-module line coverage per module using GCC/gcov/lcov and enforce the 70% threshold. Produce a separate branch-coverage artifact; line-only lcov output is not branch coverage. |
| TC-SEC-01 | Verify password-strength rejection and Argon2id salted-hash storage; ensure login output and logs contain no plaintext password. |
| TC-SEC-02 | Verify plain HTTP is refused and TLS 1.2+ is negotiated using the intended trust chain. A locally trusted development certificate is suitable only for local testing. |
| TC-SEC-03 | Verify server-side 10-minute idle expiry and immediate logout-token revocation, including replay of the old token. |
| TC-SEC-04 | Verify input validation and parameterized SQL using safe synthetic injection test strings; confirm they are rejected or inert. Do not run uncontrolled scanners against production. |
| TC-SEC-05, TC-SEC-08 | Verify login, record-view, create, and update audit events; audit log immutability and administrator-only access; no password/token/clinical payloads in audit output. |
| TC-SEC-06 | Inspect patient and clinical database columns for ciphertext; verify field encryption, distinct keys, wrong-key failure, and blind-index search behavior. |
| TC-SEC-07 | Call restricted endpoints directly with roles that lack permission and with another patient's ID. Verify server-side 403/401 regardless of visible UI controls. |

## Execution record for this recovery run

See the dated reports in `docs/reports/`. As of 2026-11-12, CTest and the Qt
client startup smoke check are recorded as executed. Manual API/security,
performance, usability, availability, and recovery cases are not marked passed
without their specified environment and evidence. Existing historical load and
coverage artifacts are identified as historical, not current reruns.
