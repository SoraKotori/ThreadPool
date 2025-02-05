project(thread_pool)

add_executable(thread_pool main.cpp)

target_compile_features(thread_pool PRIVATE cxx_std_23)
