# Stage 9 readiness and execution record

**Date:** 2026-11-16 (Asia/Calcutta)
**Repository branch:** `feature/HMS-F-060-user-account-management`
**Base revision:** `010bdf2849c73f0ad26be266f64e6a57d213bef6`
**Working tree:** already dirty before Stage 9; all prior changes and untracked files were preserved. Nothing was staged, committed, pushed, merged, rebased, reset, cleaned, or removed.
**Stage 8 result retained:** 15 PASS, 0 FAIL, 3 BLOCKED, 2 PARTIAL (75%). No Stage 9 assigned-case status was promoted.

## TC-PORT-01 — platform readiness

- Host is Windows 11 with Visual Studio Build Tools 18.11.12306.390. The installed MSVC toolset directory is `14.51.36231`; `vswhere` found no `Microsoft.VisualStudio.Component.VC.v142.x86.x64` installation. This is not MSVC 2019 (v142).
- An Ubuntu WSL distribution is listed, but `wsl --status` reports WSL1 unsupported and WSL2 unsupported because the Windows Subsystem for Linux and Virtual Machine Platform/virtualization prerequisites are disabled. No Ubuntu 22.04 shell or GCC 11 is available locally.
- Existing `.github/workflows/ci.yml` has an `ubuntu-22.04` job and a `windows-latest` job. The Linux job installs `build-essential`; the Windows job uses MSYS2 UCRT64 MinGW. It does not test MSVC. It was not triggered because the current tested changes are in the dirty local worktree, and Stage 9 forbids pushing them.
- GitHub retired its hosted `windows-2019` runner in 2025. Current standard runner labels do not provide a Windows 11/MSVC 2019 image; the official runner documentation describes Windows 11 as a larger-runner image option. References: https://github.blog/changelog/2025-04-15-upcoming-breaking-changes-and-releases-for-github-actions/ and https://docs.github.com/en/actions/reference/runners/larger-runners.
- **Result:** TC-PORT-01 remains PARTIAL. A genuine Windows 11 host with the v142 C++ build tools and a runnable Ubuntu 22.04/GCC 11 host are required to complete the official pair. The CI's Ubuntu job alone cannot complete this case.

## TC-SEC-04 — desktop/XSS readiness

- CMake's supported client uses Qt6 Widgets and Network. The existing Stage 8 fresh configure log says Qt6 was not found and skipped `hms_client`; `qmake`/`qtpaths6` and standard `C:\Qt` install locations are absent. No client was built or launched in Stage 9.
- No OWASP ZAP executable was found on PATH. The installed Java command is Oracle Java 8; no compatible scanner installation was present. No dynamic scan or screenshot was produced.
- Reused Stage 8 API evidence already verifies the XSS payload is returned as JSON and patient data is encrypted at rest. That does not prove actual desktop rendering behavior.
- **Result:** TC-SEC-04 remains PARTIAL. A supported Qt6 Widgets/Network development installation and interactive desktop session are needed to launch the client and verify the payload. A current supported Java runtime plus ZAP or an equivalent scanner is additionally needed if the official procedure requires a dynamic scan.

## TC-REL-01 — backup readiness

- Reused `PostgreSQL/backup/` implementation and deployment guide. Stage 6's synthetic encrypted backup and separate-database restore drill remains supporting evidence only.
- No staging DB connection was configured, and no approved independent backup destination or external `pgpass` file was present. The daily task was not registered or run.
- The existing task installer uses a daily 02:00 local trigger, CurrentUser DPAPI, and an interactive account. It requires the intended backup account to be logged in at runtime, and restore uses the same Windows user/profile. These are relevant deployment constraints, not production assurance.
- **Result:** TC-REL-01 remains BLOCKED. Required operator inputs: staging PostgreSQL host/port/database/least-privilege backup role, PostgreSQL data-directory path, external protected `pgpass` file if password auth is used, approved backup path on storage separate from the DB data directory, and the Windows account/DPAPI recovery plan to operate and restore the archive.

## TC-AVL-01 — monitoring readiness

- The prepared monitor validates an absolute HTTPS URL without embedded credentials and relies on standard certificate validation. It appends scheduled/observed timestamps, success, HTTP status, duration, and failure reason to CSV; it resumes using existing scheduled timestamps and records missed slots as failures when resumed late. Its task installer uses 06:00 local start, startup resume, and repeated-task restart settings.
- Fixed a missing result calculation by adding `Get-HmsSevenDayAvailabilitySummary.ps1`. After a completed seven-day run it calculates availability over the full 7 x 960 = 6,720 scheduled minute slots in `[06:00,22:00)` each day. Missing records count as failed checks; duplicate and out-of-window rows cannot increase the successful count.
- The synthetic calculation test passed with 6,717 successes, one recorded HTTP 503, two missing checks, one duplicate, and one out-of-window row: 6,717/6,720 = 99.9554%. See `availability_calculation_test.json` and `.test.txt`. This tests only the calculator; it is not staging uptime evidence.
- **Result:** TC-AVL-01 remains BLOCKED. A trusted staging HTTPS health endpoint and a persistent scheduled host/network for seven consecutive days are required. No monitoring date has started.

## TC-USA-01 — prepared materials

Added participant instructions, five fictional patient rows, blank registration timing, appointment result, unaided completion, and error/assistance templates under `usability/`. No participants were recruited, no application sessions were run, and no completion times were fabricated.

**Result:** TC-USA-01 remains BLOCKED pending five actual trained receptionists and the standardized test environment. Confirm the requested doctor slots and date availability in the isolated test system before sessions; do not use this synthetic data against production.

## Verification and status

The availability monitor, task installer, summary helper, and synthetic calculator test scripts parse under Windows PowerShell 5.1. The synthetic calculator test passed. `git diff --check` passed after the Stage 9 edits. No C++ source or Stage 8 tested production modules changed, so Stage 8 build, CTest, API, and coverage results remain the latest implementation verification. New assigned-case result: **none**. Overall result remains **15/20 = 75%**; the 90% target is not met.
