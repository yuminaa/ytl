add_library(ytd_memory
        src/allocator.cpp
        src/smart_pointer.cpp
        include/memory.h
        src/mem_op.cpp
)

target_include_directories(ytd_memory
        PUBLIC include
        PRIVATE src
)
target_link_libraries(ytd_memory PUBLIC ytd_common)