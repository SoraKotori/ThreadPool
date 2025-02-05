project(socket_server)

add_executable(socket_server main.cpp)

target_compile_features(socket_server PRIVATE cxx_std_23)
