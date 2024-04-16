cmake_minimum_required(VERSION 3.0)
project(coroutines)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    external_sort.c
    libcoro.c
    solution.c
    utils.c
    priority_queue.c
    merge_files.c
    number_file_reader.c
    insertion_sort.c)

target_include_directories(${PROJECT_NAME} 
    PRIVATE .)

target_compile_definitions(${PROJECT_NAME} 
    PRIVATE NO_CORO)

target_compile_options(${PROJECT_NAME} 
    PRIVATE -Wextra -Werror -Wall -g3 -ggdb -Wno-gnu-folding-constant)