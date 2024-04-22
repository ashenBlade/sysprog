cmake_minimum_required(VERSION 3.0)
project(userfs)

set(CMAKE_C_STANDARD 11)

set(USERFS_SOURCES
    userfs.c)

add_library(${PROJECT_NAME} SHARED)
target_sources(${PROJECT_NAME} PRIVATE ${USERFS_SOURCES})
target_compile_options(${PROJECT_NAME} PRIVATE -Wextra -Werror -Wall -Wno-gnu-folding-constant)
target_include_directories(${PROJECT_NAME} PRIVATE include)

set(TESTS_PROJECT tests)
add_executable(${TESTS_PROJECT}
    test.c)
target_link_libraries(${TESTS_PROJECT} ${PROJECT_NAME})
target_include_directories(${TESTS_PROJECT} PRIVATE include ../utils)

if(NEED_OPEN_FLAGS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NEED_OPEN_FLAGS)
    target_compile_definitions(${TESTS_PROJECT} PRIVATE NEED_OPEN_FLAGS)
endif(NEED_OPEN_FLAGS)
