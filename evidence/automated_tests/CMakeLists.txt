cmake_minimum_required(VERSION 3.20)
project(hms_tls_probe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
add_executable(hms_tls_probe hms_tls_probe.cpp)
target_include_directories(hms_tls_probe PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../vcpkg_installed/x64-windows/include")
target_link_libraries(hms_tls_probe PRIVATE
    OpenSSL::SSL OpenSSL::Crypto ws2_32)
if(MSVC AND OPENSSL_APPLINK_SOURCE AND TARGET OpenSSL::applink)
    enable_language(C)
    target_link_libraries(hms_tls_probe PRIVATE OpenSSL::applink)
endif()
