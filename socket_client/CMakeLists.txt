project(socket_client)

add_executable(socket_client main.cpp)

target_compile_features(socket_client PRIVATE cxx_std_23)
