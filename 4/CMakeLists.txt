cmake_minimum_required(VERSION 3.0)

set(LIB_NAME tpool)
set(TESTS_NAME tests)

add_library(${LIB_NAME} SHARED)
target_sources(${LIB_NAME} PRIVATE thread_pool.c task_queue.c)
target_include_directories(${LIB_NAME} PRIVATE include)

add_executable(${TESTS_NAME})
target_sources(${TESTS_NAME} PRIVATE test.c)
target_include_directories(${TESTS_NAME} PRIVATE ../utils include)
target_link_libraries(${TESTS_NAME} ${LIB_NAME})


if(NEED_DETACH)
    target_compile_definitions(${LIB_NAME} PRIVATE NEED_DETACH)
    target_compile_definitions(${TESTS_NAME} PRIVATE NEED_DETACH)
endif(NEED_DETACH)

if(NEED_TIMED_JOIN)
    target_compile_definitions(${LIB_NAME} PRIVATE NEED_TIMED_JOIN)
    # Библиотека для математики
    target_link_libraries(${LIB_NAME} m)
    target_compile_definitions(${TESTS_NAME} PRIVATE NEED_TIMED_JOIN)
endif(NEED_TIMED_JOIN)

