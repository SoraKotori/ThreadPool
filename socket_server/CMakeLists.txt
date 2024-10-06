project(socket_server)

add_executable(socket_server
    main.cpp)

set_target_properties(socket_server PROPERTIES
CXX_STANDARD 23
CXX_STANDARD_REQUIRED YES)