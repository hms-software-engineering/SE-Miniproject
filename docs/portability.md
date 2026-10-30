# Client portability

The desktop client is built as the required `hms_client` CMake target with
C++17, Qt 6 Widgets, and Qt 6 Network. Configuration fails if Qt is missing or
the compiler is older than GCC 11 or MSVC 2019.

## CI configurations

| Platform | Compiler | Qt | CI checks |
| --- | --- | --- | --- |
| Ubuntu 22.04 | GCC 11 or newer from the Ubuntu 22.04 image | Qt 6 from `qt6-base-dev` (the 2026-10-29 failed CI run resolved Qt 6.2.4) | Build `hms_client`, run CTest, launch the GUI with Qt's offscreen platform plugin |
| Windows Server 2022 hosted runner | Visual Studio 2022 x64 (MSVC 2022) | Qt 6.8.3 `win64_msvc2022_64` | Build `hms_client`, run CTest, launch the GUI with Qt's offscreen platform plugin |

The Windows job pins the Windows Server 2022 image and explicitly uses the
Visual Studio 2022 generator. Its Qt package and CMake toolchain both target
MSVC x64; MinGW is not used.

## Compiler baseline limitation

The requirement names MSVC 2019 as the minimum Windows compiler. The hosted
Windows Server 2022 image used here provides Visual Studio 2022, and the Qt
6.8.3 Windows package used by CI targets MSVC 2022. CI therefore verifies
MSVC 2022, not MSVC 2019. CMake accepts MSVC 2019 and the project is configured
for C++17, but an MSVC 2019 build remains unverified until a matching compiler
and Qt package are available for testing.

The GUI smoke check sets `QT_QPA_PLATFORM=offscreen`, allowing CI to verify
that the client starts and enters its event loop without requiring a desktop
session. It does not replace interactive acceptance testing on Windows 10/11
and Ubuntu 22.04 desktops.
