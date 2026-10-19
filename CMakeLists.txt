
cmake_minimum_required(VERSION 3.20)

project(HMS_Miniproject VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hms_server
    src/main.cpp
)

target_include_directories(hms_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
