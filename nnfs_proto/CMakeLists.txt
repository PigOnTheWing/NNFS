cmake_minimum_required(VERSION 3.16)
project(nnfs_proto C)

set(CMAKE_C_STANDARD 99)
set(LIBRARY_OUTPUT_PATH ./lib)

add_library(nnfs_proto SHARED src/request_encoder.c src/response_encoder.c)
