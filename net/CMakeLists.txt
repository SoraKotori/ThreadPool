include(FetchContent)

# 使用 FetchContent 下載 ASIO
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2  # 指定版本，可以更改成您想要的版本
)
FetchContent_MakeAvailable(asio)

# 添加您的源文件
add_executable(net main.cpp)

# 使用 target_compile_features 設定 C++ 標準
target_compile_features(net PRIVATE cxx_std_23)

target_include_directories(net PUBLIC ${asio_SOURCE_DIR}/asio/include)

# 設定 target
# target_link_libraries(MyApp PRIVATE pthread)
