cmake_minimum_required(VERSION 3.16)
project(NNFS C)

set(CMAKE_C_STANDARD 99)

add_subdirectory(./nnfs_proto ./nnfs_proto/build)
add_subdirectory(./Client ./Client/build)
add_subdirectory(./Server ./Server/build)
