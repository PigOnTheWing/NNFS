cmake_minimum_required(VERSION 3.16)
project(nnfs_server C)

set(CMAKE_C_STANDARD 99)

add_executable(nnfs_server src/main.c)

target_include_directories(nnfs_server PUBLIC ../nnfs_proto/include)
target_link_libraries(nnfs_server nnfs_proto)
