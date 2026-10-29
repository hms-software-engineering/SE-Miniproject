cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

option(HMS_ENABLE_COVERAGE "Instrument service modules for gcov coverage" OFF)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
    message(FATAL_ERROR "HMS-NF-007 requires GCC 11 or newer")
elseif(MSVC AND MSVC_VERSION LESS 1920)
    message(FATAL_ERROR "HMS-NF-007 requires MSVC 2019 or newer")
endif()

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(nlohmann_json)

# Argon2 is provided by the platform package manager.
find_path(ARGON2_INCLUDE_DIR NAMES argon2.h REQUIRED)
find_library(ARGON2_LIBRARY NAMES argon2 REQUIRED)

find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
if(NOT WIN32)
    find_package(PostgreSQL REQUIRED)
endif()

enable_testing()

# ---- HMS-NF-006 encrypted PostgreSQL backups ----
add_library(hms_backup src/backup/backup_service.cpp)
target_include_directories(hms_backup PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(hms_backup PUBLIC OpenSSL::Crypto)
add_executable(hms_backup_cli src/backup/main.cpp)
set_target_properties(hms_backup_cli PROPERTIES OUTPUT_NAME hms_backup)
target_link_libraries(hms_backup_cli PRIVATE hms_backup)
add_executable(backup_service_test tests/backup_service_test.cpp)
target_link_libraries(backup_service_test PRIVATE hms_backup)
add_test(NAME backup_service_test COMMAND backup_service_test)

# ---- Auth ----
add_library(hms_auth
    src/services/auth_service.cpp
    src/services/rbac_service.cpp
)
target_include_directories(hms_auth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(hms_auth PRIVATE ${ARGON2_INCLUDE_DIR})
target_link_libraries(hms_auth PRIVATE ${ARGON2_LIBRARY})
if(WIN32)
    target_link_libraries(hms_auth PRIVATE bcrypt)
else()
    target_link_libraries(hms_auth PRIVATE PostgreSQL::PostgreSQL OpenSSL::Crypto)
endif()

# ---- API ----
add_library(hms_api
    src/api.cpp
    src/api/audit_log_handler.cpp
    src/api/user_management_handler.cpp
)
target_include_directories(hms_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hms_api PUBLIC nlohmann_json::nlohmann_json PRIVATE hms_auth)

# Health endpoints and deterministic availability calculations.
add_library(hms_health
    src/health/health_service.cpp
    src/health/availability.cpp
)
target_include_directories(hms_health PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(api_test tests/api_test.cpp)
target_link_libraries(api_test PRIVATE hms_api)
add_test(NAME api_test COMMAND api_test)

add_executable(auth_service_test tests/auth_service_test.cpp)
target_link_libraries(auth_service_test PRIVATE hms_auth hms_api)
add_test(NAME auth_service_test COMMAND auth_service_test)

add_executable(health_test tests/health_test.cpp)
target_link_libraries(health_test PRIVATE hms_health)
add_test(NAME health_test COMMAND health_test)

# ---- SEC-001 password security ----
add_library(hms_password_security src/password_security.cpp)
target_include_directories(hms_password_security PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(hms_password_security PRIVATE ${ARGON2_INCLUDE_DIR})
target_link_libraries(hms_password_security PUBLIC ${ARGON2_LIBRARY})
if(WIN32)
    target_link_libraries(hms_password_security PRIVATE bcrypt)
endif()

add_executable(password_security_test tests/password_security_test.cpp)
target_link_libraries(password_security_test PRIVATE hms_password_security)
add_test(NAME password_security_test COMMAND password_security_test)

# ---- Appointments ----
add_library(hms_appointments src/appointments/appointments.cpp)
target_include_directories(hms_appointments PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(hms_appointments PUBLIC nlohmann_json::nlohmann_json)
if(NOT WIN32)
    target_link_libraries(hms_appointments PRIVATE PostgreSQL::PostgreSQL)
endif()

add_executable(appointments_test tests/appointments_test.cpp)
target_link_libraries(appointments_test PRIVATE hms_appointments)
add_test(NAME appointments_test COMMAND appointments_test)
add_executable(
    change_appointment_test
    tests/change_appointment_test.cpp
)

target_link_libraries(
    change_appointment_test
    PRIVATE hms_appointments
)

add_test(
    NAME change_appointment_test
    COMMAND change_appointment_test
)

# ---- F-024 Doctor daily schedule ----
add_executable(doctor_schedule_test tests/doctor_schedule_test.cpp)
target_link_libraries(doctor_schedule_test PRIVATE hms_appointments)
add_test(NAME doctor_schedule_test COMMAND doctor_schedule_test)

# ---- Clinical (AES-256-GCM) ----
add_library(hms_clinical src/clinical/clinical_service.cpp)
target_include_directories(hms_clinical PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(hms_clinical PRIVATE OpenSSL::Crypto)
if(NOT WIN32)
    target_link_libraries(hms_clinical PRIVATE PostgreSQL::PostgreSQL)
endif()

add_executable(clinical_service_test tests/clinical_service_test.cpp)
target_link_libraries(clinical_service_test PRIVATE hms_clinical hms_appointments)
add_test(NAME clinical_service_test COMMAND clinical_service_test)

# ---- Reports ----
add_library(hms_reports
    src/reports/report_service.cpp
)
target_include_directories(hms_reports PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
add_executable(report_service_test tests/report_service_test.cpp)
target_link_libraries(report_service_test PRIVATE hms_reports)
add_test(NAME report_service_test COMMAND report_service_test)

# ---- F-040 Itemized billing ----
add_library(hms_billing src/billing/billing_service.cpp)
target_include_directories(hms_billing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(NOT WIN32)
    target_link_libraries(hms_billing PRIVATE PostgreSQL::PostgreSQL)
endif()
add_executable(billing_service_test tests/billing_service_test.cpp)
target_link_libraries(billing_service_test PRIVATE hms_billing)
if(NOT WIN32)
    target_link_libraries(billing_service_test PRIVATE PostgreSQL::PostgreSQL)
endif()
add_test(NAME billing_service_test COMMAND billing_service_test)
add_executable(billing_tax_boundary_test tests/billing_tax_boundary_test.cpp)
target_link_libraries(billing_tax_boundary_test PRIVATE hms_billing)
add_test(NAME billing_tax_boundary_test COMMAND billing_tax_boundary_test)

# ---- Patients ----
add_library(hms_patients
    src/patients/patient_service.cpp
    src/patients/postgres_patient_repository.cpp
)
target_include_directories(hms_patients PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(hms_patients PRIVATE bcrypt)
else()
    target_link_libraries(hms_patients PRIVATE PostgreSQL::PostgreSQL OpenSSL::Crypto)
endif()

add_executable(patient_service_test tests/patient_service_test.cpp)
target_link_libraries(patient_service_test PRIVATE hms_patients)
add_test(NAME patient_service_test COMMAND patient_service_test)

# Clinical records
add_library(hms_clinical_records
    src/clinical/clinical.cpp
)

target_include_directories(hms_clinical_records PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hms_clinical_records PUBLIC hms_auth)

add_executable(clinical_test
    tests/clinical_test.cpp
)

target_link_libraries(clinical_test PRIVATE hms_clinical_records)

add_test(NAME clinical_test COMMAND clinical_test)

# ---- Qt desktop client ----
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
qt_add_executable(hms_client
    src/client/main.cpp
    src/client/loginwindow.h
    src/client/loginwindow.cpp
    src/client/apiclient.h
    src/client/apiclient.cpp
    src/client/patientmanagementwindow.h
    src/client/patientmanagementwindow.cpp
    src/client/workflowwindow.h
    src/client/workflowwindow.cpp
)
set_target_properties(hms_client PROPERTIES AUTOMOC ON)
target_link_libraries(hms_client PRIVATE Qt6::Widgets Qt6::Network)

# ---- TLS API server ----
add_executable(hms_server src/main.cpp)
target_link_libraries(hms_server PRIVATE
    hms_api
    hms_auth
    hms_patients
    hms_appointments
    hms_reports
    hms_billing
    hms_clinical
    hms_health
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

if(WIN32)
    target_link_libraries(hms_server PRIVATE ws2_32)
endif()

# The service inventory is explicit so every module is independently checked.
# Source files are taken from each library target to keep the coverage manifest
# in sync with the code CMake actually builds.
set(HMS_SERVICE_MODULE_TARGETS
    auth:hms_auth
    health:hms_health
    password_security:hms_password_security
    appointments:hms_appointments
    clinical:hms_clinical
    reports:hms_reports
    billing:hms_billing
    patients:hms_patients
    clinical:hms_clinical_records
    backup:hms_backup
)

if(HMS_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "HMS_ENABLE_COVERAGE requires GCC/gcov")
    endif()

    get_property(all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(candidate IN LISTS all_targets)
        get_target_property(candidate_type "${candidate}" TYPE)
        if(candidate MATCHES "^hms_"
                AND candidate_type MATCHES "^(STATIC|SHARED|MODULE)_LIBRARY$"
                AND NOT candidate STREQUAL "hms_api")
            set(candidate_registered FALSE)
            foreach(module_target IN LISTS HMS_SERVICE_MODULE_TARGETS)
                if(module_target MATCHES ":${candidate}$")
                    set(candidate_registered TRUE)
                    break()
                endif()
            endforeach()
            if(NOT candidate_registered)
                message(FATAL_ERROR
                    "Service library ${candidate} is missing from the coverage inventory")
            endif()
        endif()
    endforeach()

    set(HMS_COVERAGE_MANIFEST "${CMAKE_BINARY_DIR}/service-module-sources.txt")
    file(WRITE "${HMS_COVERAGE_MANIFEST}" "")
    foreach(module_target IN LISTS HMS_SERVICE_MODULE_TARGETS)
        string(REPLACE ":" ";" module_parts "${module_target}")
        list(GET module_parts 0 module_name)
        list(GET module_parts 1 target_name)
        if(NOT TARGET "${target_name}")
            message(FATAL_ERROR "Coverage inventory references missing target ${target_name}")
        endif()
        get_target_property(module_sources "${target_name}" SOURCES)
        set(module_source_count 0)
        foreach(source IN LISTS module_sources)
            if(source MATCHES "\\.cpp$")
                math(EXPR module_source_count "${module_source_count} + 1")
                get_filename_component(source_absolute "${source}" ABSOLUTE
                    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
                file(APPEND "${HMS_COVERAGE_MANIFEST}"
                    "${module_name}|${source_absolute}\n")
            endif()
        endforeach()
        if(module_source_count EQUAL 0)
            message(FATAL_ERROR "Coverage module ${module_name} has no C++ source files")
        endif()
        target_compile_options("${target_name}" PRIVATE --coverage -O0 -g)
    endforeach()
    foreach(coverage_executable IN ITEMS
            auth_service_test health_test password_security_test
            appointments_test change_appointment_test doctor_schedule_test
            clinical_service_test clinical_test report_service_test
            patient_service_test backup_service_test billing_service_test
            billing_tax_boundary_test hms_backup_cli hms_server)
        if(TARGET "${coverage_executable}")
            target_link_options("${coverage_executable}" PRIVATE --coverage)
        endif()
    endforeach()
    message(STATUS "GCC coverage enabled for ${HMS_SERVICE_MODULE_TARGETS}")
endif()
