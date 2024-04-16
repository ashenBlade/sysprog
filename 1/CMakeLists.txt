cmake_minimum_required(VERSION 3.0)
project(coroutines)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_executable(${PROJECT_NAME})

set(CORO_SOURCES 
    external_sort.c
    libcoro.c
    solution.c
    utils.c
    priority_queue.c
    merge_files.c
    number_file_reader.c
    insertion_sort.c)

set(CORO_COMPILE_FLAGS
    -Wextra -Werror -Wall -g3 -ggdb -Wno-gnu-folding-constant)

if(LEAK_CHECK)
    message("Обнаружена переменная LEAK_CHECK - добавляю проверку утечек")
    list(APPEND CORO_SOURCES ../utils/heap_help/heap_help.c)
    list(APPEND CORO_COMPILE_FLAGS -ldl -rdynamic)
endif(LEAK_CHECK)


target_sources(${PROJECT_NAME} PRIVATE ${CORO_SOURCES})

target_include_directories(${PROJECT_NAME} 
    PRIVATE .)

target_compile_definitions(${PROJECT_NAME} 
    PRIVATE NO_CORO)

target_compile_options(${PROJECT_NAME} 
    PRIVATE ${CORO_COMPILE_FLAGS})