cmake_minimum_required(VERSION 3.14)
project(async VERSION 0.1.0)

add_subdirectory(thread_pool)
add_subdirectory(net)
add_subdirectory(coroutine)
add_subdirectory(socket_server)