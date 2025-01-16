add_library(ytd_concurrency
        include/task.h
        src/task.cpp
)

target_include_directories(ytd_concurrency
        PUBLIC include
        PRIVATE src
)
target_link_libraries(ytd_concurrency PUBLIC ytd_common)