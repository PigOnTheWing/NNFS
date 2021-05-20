cmake_minimum_required(VERSION 3.16)
project(nnfs_client C)

set(CMAKE_C_STANDARD 99)

add_executable(nnfs_client src/main.c src/command_parser/command_parser.c)

target_include_directories(nnfs_client PUBLIC ../nnfs_proto/include)
target_link_libraries(nnfs_client nnfs_proto)
