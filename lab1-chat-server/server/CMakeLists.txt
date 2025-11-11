cmake_minimum_required(VERSION 3.10)
project(EchoServer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(echo_server echo_server.cpp)

# 编译选项
target_compile_options(echo_server PRIVATE -Wall -Wextra)

# 链接 pthread（用于多线程支持）
target_link_libraries(echo_server pthread)

