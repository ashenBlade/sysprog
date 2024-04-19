cmake_minimum_required(VERSION 3.1)

project(terminal)

set(PROJECT_SOURCES
    solution.c
    parser.c
    exec_command.c
    parse_command.c)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include)