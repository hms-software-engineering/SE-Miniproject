# System Validation Report — 2026-11-14

**Evidence snapshot:** CI and local test runs recorded through 2026-11-14.
**Documentation revision:** 2026-11-15.

## Daksha execution record — 2026-11-05

The assigned-case execution log records the tester-supplied execution timestamp
of 2026-11-05 10:00 IST for a macOS CMake build at commit
`9dc32b1edffeeb786090540b8c0d84d6e6ad7272`. The command
`ctest --test-dir build --output-on-failure` passed all 16 registered CTest
suites in 12.84 seconds. The 17 mapped functional/security records are marked
Partial because this is automated component evidence only, not a substitute
for manual acceptance. The three performance cases are deferred. See
[Test-Execution-Log-Daksha.md](Test-Execution-Log-Daksha.md) for the
per-case records, evidence references, summary, and limitations.

## Additional local verification — 2026-11-15

| Check | Result | Evidence |
| --- | --- | --- |
| Windows CTest rerun | PASS | 16/16 suites passed from the existing Visual Studio build in 12.43 seconds |
| WSL CTest rerun | PASS | 16/16 suites passed from the existing GCC build in 18.61 seconds; PostgreSQL integration flags were not enabled |
| Local API liveness and readiness | PASS | Existing HMS server returned HTTP 200 for both endpoints; readiness reported PostgreSQL `ok` |
| TLS and plaintext transport | PASS | Trusted TLS 1.2 handshake succeeded; an HTTP request to the TLS-only port received no HTTP response |
| Unauthenticated user listing | PASS | `GET /api/v1/users` returned HTTP 401 |

These live probes used the already-running local HMS server and made no
application data changes. Starting a second server instance during the check
was refused by PostgreSQL with `too many clients already`; the existing server
was left running and used for the read-only probes.

## Sprint verification summary

The recorded sprint verification passed: Windows and Ubuntu builds, all 16
registered CTest suites on both platforms, the PostgreSQL-enabled coverage
job, and a bounded four-second Qt client startup smoke check. These results
provide automated and startup evidence for the sprint deliverables.

The project owner confirms that the other 34 cases in the 40-case Test Plan
were completed through manual and automated testing. The six cases listed
below are deferred from this submission because their required extended
resources or measurement periods are not available in the current test
environment. This completion status is based on the project owner's
confirmation; individual manual execution logs were not attached.

## Environment and evidence

| Check | Result | Evidence |
| --- | --- | --- |
| Windows 2022 build and CTest | PASS | GitHub Actions run 43632305410; all 16 CTest suites passed |
| Ubuntu 22.04 build and CTest | PASS | GitHub Actions run 43632305410; all 16 CTest suites passed |
| PostgreSQL adapter/backup integration | PASS | Coverage job in run 43632305410 ran all 16 suites with integration flags enabled |
| Updated line and branch coverage capture | PASS | Run 43625440994 uploaded the branch-enabled LCOV artifact; see the code and branch coverage reports |
| Earlier local WSL CTest rerun | PASS | GCC 13.3 build; 16/16 passed in 12.06 seconds (PostgreSQL flags not enabled locally) |
| Qt client launch | PASS | Linux offscreen client remained active throughout the bounded 4-second smoke window |
| Selected live TLS/API checks | PASS | TLS health/readiness and unauthenticated user-list checks are recorded above; the complete role/route matrix is not represented by these probes |
| TC-PERF-01 — 100k-patient search load | Deferred for this submission | Requires a 100,000-patient dataset and 100 concurrent users |
| TC-PERF-02 — appointment booking load | Deferred for this submission | Requires the specified load workload and dedicated appointment test data |
| TC-PERF-03 — 200-session load rerun | Deferred for this submission | Historical result is recorded in `tests/performance/NF-003-summary.txt`; a current 30-minute rerun requires sustained load resources |
| TC-AVL-01 — availability measurement | Deferred for this submission | Requires the seven-day monitoring period specified by the Test Plan |
| TC-USA-01 — five-user usability protocol | Deferred for this submission | Requires five trained participants and the defined task protocol |
| TC-REL-01 — timed backup/recovery drill | Deferred for this submission | Requires a disposable populated database and a timed restore exercise |
| Detailed platform acceptance matrix | Partially evidenced in CI | Ubuntu 22.04 and Windows Server 2022/MSVC 2022 passed in CI; the owner reports portability checks, but detailed OS/compiler results were not included |

## Test Plan disposition

The CTest run provides component-level evidence for portions of authentication,
patients, appointments, clinical records, billing, reports, health, backup,
validation, and password security. It is not a one-to-one execution of the
40-case Appendix A.

**Passed in the scope actually executed:** Windows and Ubuntu builds; 16 CTest
suites on both platforms; 16 CTest suites in the PostgreSQL-enabled coverage
job; coverage-checker unit tests; Qt client startup.

Per the project owner's confirmation, the other 34 of 40 planned test cases
are complete. The six deferred resource-intensive cases are listed above;
they are excluded from this sprint's execution scope rather than reported as
failed.

Use the execution checklist and updated oracles in `docs/test-cases-updated.md`.
Any failures discovered during the remaining system tests should be logged as
defects with reproduction steps and synthetic test data only.
