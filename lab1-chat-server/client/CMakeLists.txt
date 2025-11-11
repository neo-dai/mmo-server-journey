cmake_minimum_required(VERSION 3.10)
project(ChatClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Protobuf
find_package(Protobuf REQUIRED)

# 设置 proto 文件路径
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/../proto)
set(PROTO_FILE ${PROTO_DIR}/chat.proto)

# 生成 Protobuf 代码
set(PROTO_SRCS ${CMAKE_CURRENT_BINARY_DIR}/chat.pb.cc)
set(PROTO_HDRS ${CMAKE_CURRENT_BINARY_DIR}/chat.pb.h)

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         --proto_path=${PROTO_DIR}
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating Protobuf C++ code"
)

# 添加可执行文件
add_executable(echo_client 
    echo_client.cpp
    ${PROTO_SRCS}
)

# 包含目录
target_include_directories(echo_client PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

# 编译选项
target_compile_options(echo_client PRIVATE -Wall -Wextra)

# 查找 absl 库（Protobuf 5.x 需要）
find_library(ABSL_LOG_INTERNAL_MESSAGE_LIB 
    NAMES absl_log_internal_message
    PATHS /opt/homebrew/lib
)

find_library(ABSL_LOG_INTERNAL_CHECK_OP_LIB 
    NAMES absl_log_internal_check_op
    PATHS /opt/homebrew/lib
)

# 链接库
target_link_libraries(echo_client 
    pthread
    ${Protobuf_LIBRARIES}
)

# 如果找到 absl 库，则链接
if(ABSL_LOG_INTERNAL_MESSAGE_LIB)
    target_link_libraries(echo_client ${ABSL_LOG_INTERNAL_MESSAGE_LIB})
endif()

if(ABSL_LOG_INTERNAL_CHECK_OP_LIB)
    target_link_libraries(echo_client ${ABSL_LOG_INTERNAL_CHECK_OP_LIB})
endif()
