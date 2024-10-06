project(coroutine)

add_executable(coroutine
    main.cpp)

    set_target_properties(coroutine PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES)