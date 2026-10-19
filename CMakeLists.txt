
cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(unofficial-argon2 CONFIG REQUIRED)

add_library(hms_api
    src/api.cpp
)

target_include_directories(hms_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(hms_password_security
    src/password_security.cpp
)

target_include_directories(hms_password_security PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hms_password_security
    PUBLIC unofficial::argon2::libargon2
    PRIVATE Bcrypt
)

add_executable(api_test
    tests/api_test.cpp
)

target_link_libraries(api_test PRIVATE hms_api)

add_executable(password_security_test
    tests/password_security_test.cpp
)

target_link_libraries(password_security_test
    PRIVATE hms_password_security
)
