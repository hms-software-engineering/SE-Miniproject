# Daksha SG — Test Execution Evidence

| Field | Value |
|---|---|
| Commit under test | `f88ab6dc5b89d02ef4ab038b87dc9e0f909a3c81` |
| Execution date | 2026-11-15 (Asia/Kolkata); timestamps are in each case record |
| Environment | macOS arm64; local HTTPS HMS server; disposable PostgreSQL 15.18; synthetic-only data |
| Build | [build.log](../automated_tests/build.log) |
| CTest | 16/16 passed; [ctest_verbose.log](../automated_tests/ctest_verbose.log) |
| Overall case results | 17 Pass; 3 performance cases Deferred |

The functional/security cases were run as live HTTPS API requests against the current build. Each case folder has two records: `status.txt` for setup, steps, expected/actual and outcome; `run.log` for sanitized requests, responses, HTTP values, and the named case check. These are API executions, not screenshots or GUI interactions. The shared Sprint videos are supplemental context and are not presented as proof of this current build. No credentials, tokens, or real patient information are recorded.

| Case | Result | Evidence |
|---|---|---|
| TC-AUTH-01 | PASS | [status](./TC-AUTH-01/status.txt), [run](./TC-AUTH-01/run.log) |
| TC-AUTH-02 | PASS | [status](./TC-AUTH-02/status.txt), [run](./TC-AUTH-02/run.log) |
| TC-AUTH-03 | PASS | [status](./TC-AUTH-03/status.txt), [run](./TC-AUTH-03/run.log) |
| TC-RBAC-01 | PASS | [status](./TC-RBAC-01/status.txt), [run](./TC-RBAC-01/run.log) |
| TC-PAT-01 | PASS | [status](./TC-PAT-01/status.txt), [run](./TC-PAT-01/run.log) |
| TC-PAT-02 | PASS | [status](./TC-PAT-02/status.txt), [run](./TC-PAT-02/run.log) |
| TC-PAT-03 | PASS | [status](./TC-PAT-03/status.txt), [run](./TC-PAT-03/run.log) |
| TC-PAT-04 | PASS | [status](./TC-PAT-04/status.txt), [run](./TC-PAT-04/run.log) |
| TC-PAT-05 | PASS | [status](./TC-PAT-05/status.txt), [run](./TC-PAT-05/run.log) |
| TC-APT-01 | PASS | [status](./TC-APT-01/status.txt), [run](./TC-APT-01/run.log) |
| TC-APT-02 | PASS | [status](./TC-APT-02/status.txt), [run](./TC-APT-02/run.log) |
| TC-APT-03 | PASS | [status](./TC-APT-03/status.txt), [run](./TC-APT-03/run.log) |
| TC-APT-04 | PASS | [status](./TC-APT-04/status.txt), [run](./TC-APT-04/run.log) |
| TC-CLN-01 | PASS | [status](./TC-CLN-01/status.txt), [run](./TC-CLN-01/run.log) |
| TC-CLN-02 | PASS | [status](./TC-CLN-02/status.txt), [run](./TC-CLN-02/run.log) |
| TC-CLN-03 | PASS | [status](./TC-CLN-03/status.txt), [run](./TC-CLN-03/run.log) |
| TC-CLN-04 | PASS | [status](./TC-CLN-04/status.txt), [run](./TC-CLN-04/run.log) |
| TC-PERF-01 | DEFERRED | [status](./TC-PERF-01/status.txt) |
| TC-PERF-02 | DEFERRED | [status](./TC-PERF-02/status.txt) |
| TC-PERF-03 | DEFERRED | [status](./TC-PERF-03/status.txt) |
