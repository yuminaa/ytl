add_library(ytd_algorithm INTERFACE)

target_include_directories(ytd_algorithm
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(ytd_algorithm
        INTERFACE
        include/algorithm.h
        include/search.inl
        include/sequence.inl
        include/sort.inl
        include/compare.inl
        include/heap.inl
        include/numeric.inl
)

target_link_libraries(ytd_algorithm INTERFACE ytd_common)