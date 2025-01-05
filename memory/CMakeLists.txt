add_library(ytd_memory
        include/memory.h
        include/allocator.inl
        include/smart_ptr.inl

        src/mem_op.cpp
)

target_include_directories(ytd_memory
        PUBLIC include
        PRIVATE src
)
target_link_libraries(ytd_memory PUBLIC ytd_common)