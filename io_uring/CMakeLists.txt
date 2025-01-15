project(io_uring)

add_executable(io_uring
    main.cpp)

set_target_properties(io_uring PROPERTIES
CXX_STANDARD 23
CXX_STANDARD_REQUIRED YES)