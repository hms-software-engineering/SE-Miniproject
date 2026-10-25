
cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_path(ARGON2_INCLUDE_DIR NAMES argon2.h REQUIRED)
find_library(ARGON2_LIBRARY NAMES argon2 REQUIRED)

add_library(hms_auth
    src/services/auth_service.cpp
    src/services/rbac_service.cpp
)

target_include_directories(hms_auth PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(hms_auth PRIVATE ${ARGON2_INCLUDE_DIR})
target_link_libraries(hms_auth PRIVATE ${ARGON2_LIBRARY})

if(WIN32)
    target_link_libraries(hms_auth PRIVATE bcrypt)
else()
    find_package(PostgreSQL REQUIRED)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    target_link_libraries(hms_auth PRIVATE
        PostgreSQL::PostgreSQL
        OpenSSL::Crypto
    )
endif()

add_executable(auth_service_test
    tests/auth_service_test.cpp
)
target_link_libraries(auth_service_test PRIVATE hms_auth)

# API library
add_library(hms_api
    src/api.cpp
)

target_include_directories(hms_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# API test
add_executable(api_test
    tests/api_test.cpp
)

target_link_libraries(api_test PRIVATE hms_api)

# Appointments library
add_library(hms_appointments
    src/appointments/appointments.cpp
)

target_include_directories(hms_appointments PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(NOT WIN32)
    target_link_libraries(hms_appointments PRIVATE PostgreSQL::PostgreSQL)
endif()

# Clinical records are encrypted with AES-256-GCM before they are persisted.
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
add_library(hms_clinical
    src/clinical/clinical_service.cpp
)
target_include_directories(hms_clinical PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(hms_clinical PRIVATE OpenSSL::Crypto)
if(NOT WIN32)
    target_link_libraries(hms_clinical PRIVATE PostgreSQL::PostgreSQL)
endif()

# Appointments test
add_executable(appointments_test
    tests/appointments_test.cpp
)

target_link_libraries(appointments_test PRIVATE hms_appointments)

# Enable CTest
enable_testing()

add_test(NAME api_test COMMAND api_test)
add_test(NAME appointments_test COMMAND appointments_test)
add_test(NAME auth_service_test COMMAND auth_service_test)

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
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
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

add_executable(clinical_service_test
    tests/clinical_service_test.cpp
)
target_link_libraries(clinical_service_test PRIVATE hms_clinical)
add_test(NAME clinical_service_test COMMAND clinical_service_test)

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
        src/client/workflowwindow.h
        src/client/workflowwindow.cpp
    )

    target_link_libraries(hms_client PRIVATE
        Qt6::Widgets
        Qt6::Network
    )
else()
    message(STATUS "Qt6 Widgets and Network not found; skipping hms_client")
endif()

# API server (Windows only)
if(WIN32)
    find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
    add_executable(hms_server
        src/main.cpp
    )

    target_link_libraries(hms_server PRIVATE
        hms_api
        hms_auth
        hms_patients
        hms_appointments
        hms_clinical
        OpenSSL::SSL
        OpenSSL::Crypto
        ws2_32
    )
endif()
