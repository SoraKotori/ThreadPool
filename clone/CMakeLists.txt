project(clone)

add_executable(clone main.cpp)

target_compile_features(clone PRIVATE cxx_std_23)