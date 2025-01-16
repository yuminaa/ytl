add_library(ytd_network
        include/https.h
        include/udp.h

        src/https.cpp
        src/udp.cpp
)

target_include_directories(ytd_network
        PUBLIC include
        PRIVATE src
)
target_link_libraries(ytd_network PUBLIC ytd_common)