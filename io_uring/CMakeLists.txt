project(io_uring)

add_executable(io_uring main.cpp)

target_compile_features(io_uring PRIVATE cxx_std_23)
target_link_libraries  (io_uring PRIVATE uring)