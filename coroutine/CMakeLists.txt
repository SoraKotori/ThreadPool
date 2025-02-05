project(coroutine)

add_executable(coroutine main.cpp)

target_compile_features(coroutine PRIVATE cxx_std_23)
