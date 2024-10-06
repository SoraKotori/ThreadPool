project(thread_pool)

add_executable(thread_pool
    main.cpp)

set_target_properties(thread_pool PROPERTIES
CXX_STANDARD 23
CXX_STANDARD_REQUIRED YES)
