cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

set(nlohmann_json_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/nlohmann_json")
find_package(nlohmann_json CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use the Argon2 dependency installed through vcpkg.
set(ARGON2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
find_library(ARGON2_LIBRARY NAMES argon2 libargon2
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib"
    NO_DEFAULT_PATH
    REQUIRED
)
add_library(hms_api
    src/api.cpp
    src/services/auth_service.cpp
    src/services/rbac_service.cpp
)

target_include_directories(hms_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ARGON2_INCLUDE_DIR}
)

target_link_libraries(hms_api PUBLIC
    ${ARGON2_LIBRARY}
)
add_executable(hms_server
    src/main.cpp
)

target_link_libraries(hms_server PRIVATE hms_api nlohmann_json::nlohmann_json)

add_custom_command(TARGET hms_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/argon2.dll"
        "$<TARGET_FILE_DIR:hms_server>"
)
add_executable(api_test
    tests/api_test.cpp
)

target_link_libraries(api_test PRIVATE hms_api)
add_custom_command(TARGET api_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin/argon2.dll"
        "$<TARGET_FILE_DIR:api_test>"
)