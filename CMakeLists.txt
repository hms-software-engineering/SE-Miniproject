cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
if(WIN32)
    find_package(libpqxx CONFIG REQUIRED)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBPQXX REQUIRED IMPORTED_TARGET libpqxx)
endif()

find_path(ARGON2_INCLUDE_DIR NAMES argon2.h REQUIRED)
find_library(ARGON2_LIBRARY NAMES argon2 REQUIRED)

find_package(httplib CONFIG QUIET)

if(NOT TARGET httplib::httplib)
    find_path(HTTPLIB_INCLUDE_DIR NAMES httplib.h REQUIRED)

    add_library(httplib::httplib INTERFACE IMPORTED)
    set_target_properties(httplib::httplib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${HTTPLIB_INCLUDE_DIR}"
    )
endif()

enable_testing()

# Authentication and RBAC
add_library(hms_auth
    src/services/auth_service.cpp
    src/services/rbac_service.cpp
)

target_include_directories(hms_auth PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(hms_auth PRIVATE bcrypt)
else()
    target_link_libraries(hms_auth PRIVATE
        OpenSSL::Crypto
    )
endif()

add_executable(auth_service_test
    tests/auth_service_test.cpp
)
target_link_libraries(auth_service_test PRIVATE hms_auth)
add_test(NAME auth_service_test COMMAND auth_service_test)

# API library
add_library(hms_api
    src/api.cpp
)

target_include_directories(hms_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(hms_api PRIVATE
    ${ARGON2_INCLUDE_DIR}
)

target_link_libraries(hms_api PUBLIC
    hms_auth
    ${ARGON2_LIBRARY}
    OpenSSL::Crypto
)

if(NOT WIN32)
    find_package(PostgreSQL REQUIRED)
    target_link_libraries(hms_api PRIVATE PostgreSQL::PostgreSQL)
endif()


# API test
add_executable(api_test
    tests/api_test.cpp
)
target_link_libraries(api_test PRIVATE
    hms_api
    nlohmann_json::nlohmann_json
)
add_test(NAME api_test COMMAND api_test)

# Appointments library
add_library(hms_appointments
    src/appointments/appointments.cpp
    src/appointments/postgres_appointment_repository.cpp
)

target_include_directories(hms_appointments PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hms_appointments PUBLIC
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(hms_appointments PUBLIC libpqxx::pqxx)
else()
    target_link_libraries(hms_appointments PUBLIC PkgConfig::LIBPQXX)
endif()

# Appointments test
add_executable(appointments_test
    tests/appointments_test.cpp
)
target_link_libraries(appointments_test PRIVATE hms_appointments)
add_test(NAME appointments_test COMMAND appointments_test)

# Patient registration and search
add_library(hms_patients
    src/patients/patient_service.cpp
    src/patients/postgres_patient_repository.cpp
)

target_include_directories(hms_patients PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(hms_patients PRIVATE bcrypt)
else()
    find_package(PostgreSQL REQUIRED)
    target_link_libraries(hms_patients PRIVATE
        PostgreSQL::PostgreSQL
        OpenSSL::Crypto
    )
endif()

add_executable(patient_service_test
    tests/patient_service_test.cpp
)
target_link_libraries(patient_service_test PRIVATE hms_patients)
add_test(NAME patient_service_test COMMAND patient_service_test)

# HTTP server
add_executable(hms_server
    src/main.cpp
)

target_link_libraries(hms_server PRIVATE
    hms_api
    hms_auth
    hms_patients
    hms_appointments
    nlohmann_json::nlohmann_json
    httplib::httplib
)

if(WIN32)
    target_link_libraries(hms_server PRIVATE ws2_32)
endif()

# Qt desktop client (built when Qt6 is available)
find_package(Qt6 QUIET COMPONENTS Widgets Network)

if(Qt6_FOUND)
    qt_standard_project_setup()

    qt_add_executable(hms_client
        src/client/main.cpp
        src/client/loginwindow.h
        src/client/loginwindow.cpp
        src/client/apiclient.h
        src/client/apiclient.cpp
        src/client/patientmanagementwindow.h
        src/client/patientmanagementwindow.cpp
    )

    target_link_libraries(hms_client PRIVATE
        Qt6::Widgets
        Qt6::Network
    )
else()
    message(STATUS "Qt6 Widgets and Network not found; skipping hms_client")
endif()
