# Hospital Management System

This repository contains the C++17 HMS API server, Qt 6 desktop client,
PostgreSQL schema, service tests, and encrypted-backup utility. The requirement
review and the items that still need operational or UI work are in
[docs/requirements-status.md](docs/requirements-status.md).

## Requirements and tools

- CMake 3.20 or newer and a C++17 compiler (GCC 11+ or MSVC 2019+).
- Qt 6 with Widgets and Network for `hms_client`.
- OpenSSL, Argon2, and PostgreSQL 15. On Windows, keep the PostgreSQL
  `bin` directory and the OpenSSL/Argon2 runtime DLL directories on `PATH`.
- Docker Desktop/Engine with Compose to run the local PostgreSQL database.
- A locally trusted TLS certificate for `localhost` and `127.0.0.1`.
  `mkcert` is a convenient development-only option; do not disable certificate
  verification.

The Windows CI configuration uses Visual Studio 2022 x64, Qt 6.8.3, and
vcpkg. MSVC 2019 is accepted by CMake but is not the Windows CI compiler; see
[docs/portability.md](docs/portability.md).

## Configure and build on Windows

Run these commands from the repository root in PowerShell. In Visual Studio
Installer, modify the installed Visual Studio Community 2022 instance and add
the **Desktop development with C++** workload (including the MSVC x64/x86
build tools and a Windows SDK). A Visual Studio installation without this
workload cannot build the project.

Install Qt 6 for MSVC 2022 x64 using the Qt installer, and install Git if it
is not already available. CI is configured with Qt 6.8.3; a local Qt
6.12.0 MSVC 2022 x64 build has also been verified. Bootstrap vcpkg if you do
not already have it:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
& C:\vcpkg\bootstrap-vcpkg.bat
```

Set `VCPKG_ROOT` and `QTDIR` to the actual installation directories. The
following defaults apply only if those directories exist on your machine:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
$env:QTDIR = 'C:\Qt\6.12.0\msvc2022_64'

if (-not (Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")) {
  throw 'Set VCPKG_ROOT to the directory containing the vcpkg toolchain.'
}
if (-not (Test-Path "$env:QTDIR\lib\cmake\Qt6")) {
  throw 'Set QTDIR to the installed Qt 6 MSVC 2022 x64 directory.'
}

cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:QTDIR" `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The vcpkg manifest installs the declared C++ dependencies during CMake
configuration. If the manifest dependencies are already installed, you can
skip to the build and test commands.

## Start the database and application on Windows

Start Docker Desktop, then from the repository root:

```powershell
docker compose -f PostgreSQL\docker-compose.yml up -d
docker compose -f PostgreSQL\docker-compose.yml ps
```

Wait for the `db` service to report healthy. The local Compose defaults are
database `hms`, user `hms_app`, and password `local_dev_only_change_me`. These
are development-only values. If you change `POSTGRES_PASSWORD`, set the same
value in `PGPASSWORD`; changing it after the named database volume has already
been initialized also requires updating the database user's password.

Create a trusted local certificate (one-time setup):

```powershell
mkcert -install
New-Item -ItemType Directory -Force certs | Out-Null
mkcert -cert-file certs\localhost.pem `
  -key-file certs\localhost-key.pem localhost 127.0.0.1
```

Configure the server in the PowerShell window that will run it. The example
keys are for local testing only; use distinct, randomly generated secret values
outside a disposable development environment.

```powershell
$env:PGHOST = 'localhost'
$env:PGPORT = '5432'
$env:PGDATABASE = 'hms'
$env:PGUSER = 'hms_app'
$env:PGPASSWORD = 'local_dev_only_change_me'

# Each value below is exactly 32 ASCII bytes and the three values are distinct.
$env:HMS_PATIENT_DATA_KEY = 'dev-data-key-32bytes-12345678901'
$env:HMS_PATIENT_NAME_INDEX_KEY = 'dev-name-key-32bytes-12345678901'
$env:HMS_PATIENT_PHONE_INDEX_KEY = 'dev-phone-key-32bytes-1234567890'
$env:HMS_BILL_TAX_RATE = '18.00'
$env:HMS_TLS_CERT_FILE = (Resolve-Path .\certs\localhost.pem).Path
$env:HMS_TLS_KEY_FILE = (Resolve-Path .\certs\localhost-key.pem).Path
$env:HMS_BOOTSTRAP_ADMIN_USERNAME = 'hms_admin'
$env:HMS_BOOTSTRAP_ADMIN_PASSWORD = 'ChangeThis-Dev-Only-934!'

.\build\Release\hms_server.exe
```

Open a second PowerShell window at the repository root and start the desktop
client:

```powershell
$env:HMS_API_BASE_URL = 'https://localhost:8443'
.\build\Release\hms_client.exe
```

Sign in using the bootstrap administrator credentials configured above. The
development seed accounts in the database have intentionally invalid password
hashes and cannot be used to sign in. The bootstrap admin can create test
accounts through the administrator workflow.

## Build and run on Ubuntu 22.04

Install the system dependencies, Qt, Docker/Compose, and a trusted local
certificate tool such as `mkcert`:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev libpq-dev \
  nlohmann-json3-dev libargon2-dev qt6-base-dev libgl1-mesa-dev \
  libxkbcommon-dev docker.io docker-compose-v2
```

Configure and build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Start the database from the repository root:

```sh
docker compose -f PostgreSQL/docker-compose.yml up -d
docker compose -f PostgreSQL/docker-compose.yml ps
```

Configure TLS once:

```sh
mkcert -install
mkdir -p certs
mkcert -cert-file certs/localhost.pem \
  -key-file certs/localhost-key.pem localhost 127.0.0.1
```

Start the server in one terminal. As on Windows, the key examples are
development-only and should be replaced with distinct random secrets:

```sh
export PGHOST=localhost PGPORT=5432 PGDATABASE=hms
export PGUSER=hms_app PGPASSWORD=local_dev_only_change_me
export HMS_PATIENT_DATA_KEY='dev-data-key-32bytes-12345678901'
export HMS_PATIENT_NAME_INDEX_KEY='dev-name-key-32bytes-12345678901'
export HMS_PATIENT_PHONE_INDEX_KEY='dev-phone-key-32bytes-1234567890'
export HMS_BILL_TAX_RATE=18.00
export HMS_TLS_CERT_FILE="$PWD/certs/localhost.pem"
export HMS_TLS_KEY_FILE="$PWD/certs/localhost-key.pem"
export HMS_BOOTSTRAP_ADMIN_USERNAME=hms_admin
export HMS_BOOTSTRAP_ADMIN_PASSWORD='ChangeThis-Dev-Only-934!'
./build/hms_server
```

In another terminal:

```sh
export HMS_API_BASE_URL=https://localhost:8443
./build/hms_client
```

## Database checks and additional targets

To run the PostgreSQL schema verification against the local development
database:

```powershell
Get-Content -Raw PostgreSQL\database\tests\verify.sql |
  docker compose -f PostgreSQL\docker-compose.yml exec -T db `
    psql -U hms_app -d hms
```

On Linux, use:

```sh
docker compose -f PostgreSQL/docker-compose.yml exec -T db \
  psql -U hms_app -d hms < PostgreSQL/database/tests/verify.sql
```

The executable targets include `hms_server`, `hms_client`, and `hms_backup`.
The encrypted-backup scheduler is not installed automatically; review
[PostgreSQL/BACKUP-RECOVERY.md](PostgreSQL/BACKUP-RECOVERY.md) before using it.
Never use development data or the example secrets for production.
