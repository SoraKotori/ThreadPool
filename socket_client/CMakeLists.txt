project(socket_client)

add_executable(socket_client
    main.cpp)

set_target_properties(socket_client PROPERTIES
CXX_STANDARD 23
CXX_STANDARD_REQUIRED YES)