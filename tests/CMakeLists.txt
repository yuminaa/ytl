add_library(catch2 STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/catch2/catch2.cpp
)
target_include_directories(catch2 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/catch2
)

add_executable(ytd_tests
        test.cpp
)

target_link_libraries(ytd_tests
        PRIVATE
        ytd_algorithm
        # ytd_memory
        ytd_string
        # catch2
)

add_test(NAME ytd_tests COMMAND ytd_tests)