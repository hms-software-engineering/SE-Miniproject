# Service-module coverage

The Ubuntu coverage job builds with GCC instrumentation, runs the CTest suites
with the PostgreSQL repository tests enabled, and uses `gcov` through `lcov` to
measure executable source lines. Service libraries in `CMakeLists.txt` are
listed in the coverage inventory; their C++ sources are taken directly from
the CMake targets. The two clinical libraries are grouped into one clinical
module. Tests, generated dependencies, the API, desktop client, and server
entry point are excluded from the service-module report.
The CI PostgreSQL setup also exercises a real `pg_dump`/`pg_restore` backup and
restore against the disposable `hms_backup_restore_test` database.

For a local Linux build with GCC and LCOV installed:

```sh
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DHMS_ENABLE_COVERAGE=ON
cmake --build build-coverage --parallel
ctest --test-dir build-coverage --output-on-failure
mkdir -p build-coverage/coverage
lcov --capture --directory build-coverage --base-directory . \
  --output-file build-coverage/coverage/raw.info --rc branch_coverage=0
lcov --remove build-coverage/coverage/raw.info \
  '*/tests/*' '*/_deps/*' '*/src/main.cpp' '*/src/client/*' '*/src/api.cpp' \
  --output-file build-coverage/coverage/service-modules.info \
  --rc branch_coverage=0
python3 scripts/check_service_coverage.py \
  --manifest build-coverage/service-module-sources.txt \
  --lcov build-coverage/coverage/service-modules.info --threshold 70
```

The checker reports executable and covered lines separately for each module.
Missing per-source records and modules below 70% fail the command with the
module name and measured percentage (or the missing source path). The coverage
build is opt-in; ordinary CMake builds do not receive coverage flags.

The CI coverage job also needs PostgreSQL 15. It applies the migrations under
`PostgreSQL/database/migrations/` and enables the optional PostgreSQL-backed
tests with test-only keys. For the backup test, create a separate empty restore
database and set `HMS_BACKUP_TEST_POSTGRES=1` and
`HMS_BACKUP_TEST_RESTORE_DATABASE` to its name. Set the corresponding
`HMS_*_TEST_POSTGRES` flags, database connection variables, and 32-byte keys to
exercise those adapters locally.
