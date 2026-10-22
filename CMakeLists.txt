
cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-argon2 CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(hms_api
    src/api.cpp
    src/services/auth_service.cpp
    src/services/rbac_service.cpp
)

target_include_directories(hms_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hms_api PUBLIC
    unofficial::argon2::libargon2
    OpenSSL::Crypto
)

add_executable(hms_server
    src/main.cpp
)

target_link_libraries(hms_server PRIVATE
    hms_api
    nlohmann_json::nlohmann_json
    httplib::httplib
)

add_executable(api_test
    tests/api_test.cpp
)

target_link_libraries(api_test PRIVATE
    hms_api
    nlohmann_json::nlohmann_json
)
