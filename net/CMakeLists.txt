add_executable(net main.cpp)

target_compile_features(net PRIVATE cxx_std_23)

target_link_libraries(net PRIVATE asio)
