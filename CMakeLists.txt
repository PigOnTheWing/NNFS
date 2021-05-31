cmake_minimum_required(VERSION 3.16)
project(NNFS C)

set(CMAKE_C_STANDARD 99)

add_subdirectory(./nnfs_proto ./nnfs_proto)
add_subdirectory(./Client ./Client)
add_subdirectory(./Server ./Server)
add_subdirectory(./test ./test)
