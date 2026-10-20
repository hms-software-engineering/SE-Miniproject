
cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Appointments test
add_executable(appointments_test
    tests/appointments_test.cpp
)

target_link_libraries(appointments_test PRIVATE hms_appointments)

# Enable CTest
enable_testing()

add_test(NAME appointments_test COMMAND appointments_test)

# Qt desktop client
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(hms_client
    src/client/main.cpp
    src/client/loginwindow.h
    src/client/loginwindow.cpp
    src/client/apiclient.h
    src/client/apiclient.cpp
)

target_link_libraries(hms_client PRIVATE
    Qt6::Widgets
    Qt6::Network
)

# API server (Windows only)
if(WIN32)
    add_executable(hms_server
        src/main.cpp
    )

    target_link_libraries(hms_server PRIVATE
        hms_api
        ws2_32
    )
endif()
