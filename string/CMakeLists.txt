add_library(ytd_string
        src/string.cpp
        include/string.h
)

target_include_directories(ytd_string
        PUBLIC include
        PRIVATE src
)
target_link_libraries(ytd_string PUBLIC ytd_common)