cmake_minimum_required(VERSION 3.16)
project(nnfs_client C)

set(CMAKE_C_STANDARD 99)

add_executable(rw_test rw_test.c)

target_include_directories(rw_test PUBLIC ../nnfs_proto/include)
target_link_libraries(rw_test nnfs_proto)

add_executable(unit_tests unit_tests.c)

target_include_directories(unit_tests PUBLIC ../nnfs_proto/include)
target_link_libraries(unit_tests nnfs_proto)

configure_file(command_test.sh command_test.sh COPYONLY)
file (COPY command_test.sh
        DESTINATION .
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
