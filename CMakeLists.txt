cmake_minimum_required(VERSION 3.28)
project(ThreadPool)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${CMAKE_SOURCE_DIR}/asio/asio/include)

add_subdirectory(clone)
add_subdirectory(coroutine)
add_subdirectory(io_uring)
add_subdirectory(net)
add_subdirectory(socket_server)
add_subdirectory(socket_client)
add_subdirectory(thread_pool)
