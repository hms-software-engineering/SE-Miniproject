cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(hms_api
    src/api.cpp
)

target_include_directories(hms_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(api_test
    tests/api_test.cpp
)

target_link_libraries(api_test PRIVATE hms_api)
enable_testing()

add_library(hms_appointments
    src/appointments/appointments.cpp
)

target_include_directories(hms_appointments PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(appointments_test
    tests/appointments_test.cpp
)

target_link_libraries(appointments_test PRIVATE hms_appointments)

add_test(NAME appointments_test COMMAND appointments_test)
