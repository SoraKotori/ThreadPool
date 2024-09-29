cmake_minimum_required(VERSION 3.14)

include(FetchContent)

# 使用 FetchContent 下載 ASIO
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2  # 指定版本，可以更改成您想要的版本
)
FetchContent_MakeAvailable(asio)

# 添加您的源文件
add_executable(MyApp main.cpp)

# 使用 set_target_properties 設定 C++ 標準
set_target_properties(MyApp PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES)

target_include_directories(MyApp PUBLIC ${asio_SOURCE_DIR}/asio/include)

# 設定 target
# target_link_libraries(MyApp PRIVATE pthread)
