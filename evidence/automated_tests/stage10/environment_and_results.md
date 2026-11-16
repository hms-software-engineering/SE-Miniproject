# HMS Stage 10 — platform and desktop security results

Date: 2026-11-16 (Asia/Calcutta)

## Preserved repository state

- Root: `C:/Users/dhruv/Downloads/HMS-Team-Repo`
- Branch: `feature/HMS-F-060-user-account-management`
- HEAD: `010bdf2849c73f0ad26be266f64e6a57d213bef6` (`010bdf2 merge: integrate latest main into SEC-007 authorization`)
- The tree was already dirty, with changes to `CMakeLists.txt`, `PostgreSQL/README.md`, `src/appointments/*`, `src/client/*`, `src/clinical/*`, `src/main.cpp`, `src/services/auth_service.*`, and tests; untracked local build directories, backup/patch files, `docs/`, `evidence/`, and `src/billing/`, `src/health/`, and `tests/billing_service_test.cpp`, `tests/health_test.cpp` were also present. These were preserved.
- `git diff --stat` at inspection: 16 tracked files changed, 1,442 insertions and 16 deletions. `git diff --check` passed. No staged changes were present. Stage 10 added no application source, CMake, vcpkg, or CI workflow changes.

## TC-PORT-01

Required: Windows 11/MSVC 2019 and Ubuntu 22.04/GCC 11.

- Local Visual Studio installation: Build Tools 18.11.12306.390; installed MSVC toolset directory `14.51.36231`; `vswhere` found no `Microsoft.VisualStudio.Component.VC.v142.x86.x64` installation. This is not the required MSVC 2019 toolset.
- The existing `.github/workflows/ci.yml` runs `ubuntu-22.04` and `windows-latest`. Linux installs `build-essential`; Windows explicitly uses MSYS2 UCRT64/MinGW. It has no MSVC job.
- The local Ubuntu WSL distribution cannot start: WSL reports WSL1 unsupported and WSL2 prerequisites (Windows Subsystem for Linux/Virtual Machine Platform/virtualization) disabled.
- No suitable approved Windows 11/MSVC 2019 runner was configured in the checked-in workflow. The public GitHub Actions run visible during inspection was for Daksha's PR #110; it is unrelated to this dirty worktree and was not rerun or modified. The connected read-only GitHub interface does not expose the organization's approved self-hosted runner inventory.
- CI cannot test this uncommitted local worktree without publishing its changes. Stage 10 did not push, create a temporary remote branch, or run against a different commit and call it compliance.
- **Result:** TC-PORT-01 remains PARTIAL. No Stage 10 Windows/MSVC 2019 or Ubuntu 22.04/GCC 11 build or smoke test was run. Stage 8 Clang-cl evidence remains supporting evidence only.

## TC-SEC-04

- CMake calls `find_package(Qt6 QUIET COMPONENTS Widgets Network)` with no explicit Qt6 minimum version. The required modules are **Widgets** and **Network**.
- The project's vcpkg manifest contains `argon2`, `libpq`, and `openssl`; it does not declare Qt. No `qmake`, `qtpaths6`, or Qt installation directory was found. The Stage 8 CMake configure log explicitly says Qt6 Widgets and Network were not found and `hms_client` was skipped.
- No project-supported Qt package was already available, so Stage 10 did not bootstrap a new package manager, edit the manifest, install unapproved software, or alter global settings. Per instruction, no repeated installation attempts were made.
- The desktop client was not built or launched. No desktop payload interaction, rendered-output encoding check, manipulation attempt, or screenshot was performed. No local ZAP executable was available on PATH; no dynamic scan was run.
- Reused prior SQL injection evidence from Stage 6/8 rather than repeating it. API-level XSS JSON/encryption results remain supporting evidence only.
- **Result:** TC-SEC-04 remains PARTIAL.

## Regression and current case result

Stage 10 changed no C++/shared application components; no fresh CTest run was warranted. Stage 8 remains the latest relevant application regression: fresh Windows build 38/38, normal CTest 8/8, isolated PostgreSQL service integration tests 3/3, API regression passed, and reported source line coverage remained above 70% for every measured module. Stage 9's availability calculator and usability-template checks are unchanged.

Assigned-case status remains 15 PASS, 0 FAIL, 3 BLOCKED, and 2 PARTIAL (75%). Stage 10 completed no assigned procedure and promoted no case. `git diff --check` passed after documentation/evidence updates; no changes were staged, committed, or pushed.
