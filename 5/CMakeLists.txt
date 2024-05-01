cmake_minimum_required(VERSION 3.0)
project(chat VERSION 1.0)

set(LIB_NAME chat)
set(SERVER_NAME server)
set(CLIENT_NAME client)
set(TESTS_NAME tests)

include_directories(include)
add_compile_options(-Wextra -Werror -Wall -Wno-gnu-folding-constant)

add_library(${LIB_NAME} SHARED)
target_sources(${LIB_NAME} PRIVATE
    chat.c
    chat_client.c
    chat_server.c
    recv_buf.c
    send_queue.c
    queue.c)

add_executable(${SERVER_NAME})
target_sources(${SERVER_NAME} PRIVATE
    chat_server_exe.c)
target_link_libraries(${SERVER_NAME} PRIVATE ${LIB_NAME})

add_executable(${CLIENT_NAME})
target_link_libraries(${CLIENT_NAME} PRIVATE ${LIB_NAME})
target_sources(${CLIENT_NAME} PRIVATE
    chat_client_exe.c)

add_executable(${TESTS_NAME})
target_sources(${TESTS_NAME} PRIVATE
    test.c)
target_link_libraries(${TESTS_NAME} ${LIB_NAME})
target_include_directories(${TESTS_NAME} PRIVATE ../utils)