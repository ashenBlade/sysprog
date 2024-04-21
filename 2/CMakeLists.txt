cmake_minimum_required(VERSION 3.1)
project(terminal)

set(CMAKE_C_STANDARD 99)

set(PROJECT_SOURCES
    solution.c
    parser.c
    exec_command.c
    parse_command.c
    builtin_command.c)

set(PROJECT_COMPILE_FLAGS
    -Wextra
    -Werror
    -Wall
    -g3
    -ggdb
    -Wno-gnu-folding-constant)

if(LEAK_CHECK)
    message("Обнаружена переменная LEAK_CHECK - добавляю поддержку проверки утечки")
    list(APPEND PROJECT_SOURCES ../utils/heap_help/heap_help.c)
    list(APPEND PROJECT_COMPILE_FLAGS -ldl -rdynamic -DLEAK_CHECK)
endif(LEAK_CHECK)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_compile_options(${PROJECT_NAME} PRIVATE ${PROJECT_COMPILE_FLAGS})