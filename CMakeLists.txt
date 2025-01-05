cmake_minimum_required(VERSION 3.29)
project(ytl
        LANGUAGES CXX
        VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(YTD_BUILD_TESTS "Build test suite" ON)

add_library(ytd_common INTERFACE)
target_include_directories(ytd_common INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(algorithm)
add_subdirectory(memory)
add_subdirectory(string)

if(YTD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()