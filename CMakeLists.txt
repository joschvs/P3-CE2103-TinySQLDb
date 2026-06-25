cmake_minimum_required(VERSION 3.16)
project(P3_CE2103_TinySQLDb)

set(CMAKE_CXX_STANDARD 20)

add_executable(P3_CE2103_TinySQLDb
    server/main.cpp
    server/QueryProcessor.cpp
    server/WebAPI.cpp
)

target_include_directories(P3_CE2103_TinySQLDb PRIVATE server)

# En Windows linkear con winsock (necesario para httplib)
if(WIN32)
    target_link_libraries(P3_CE2103_TinySQLDb ws2_32)
endif()