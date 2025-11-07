cmake_minimum_required(VERSION 4.0)
project(mrezno_programiranje)

set(CMAKE_CXX_STANDARD 17)

add_executable(server server.cpp)
add_executable(klijent client.cpp)
if (WIN32)
    target_link_libraries(server ws2_32)
    target_link_libraries(klijent ws2_32)
endif()

