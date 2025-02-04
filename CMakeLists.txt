cmake_minimum_required(VERSION 3.28)
project(async VERSION 0.1.0)

add_subdirectory(coroutine)
add_subdirectory(io_uring)
add_subdirectory(net)
add_subdirectory(socket_server)
add_subdirectory(socket_client)
add_subdirectory(thread_pool)
