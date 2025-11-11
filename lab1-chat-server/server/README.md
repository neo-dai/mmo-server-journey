# Chat Server (C++)

一个支持多用户聊天的 C++ 服务器，使用 Protobuf 协议规范化消息格式。

## 功能特性

- ✅ TCP Socket 服务器
- ✅ 多客户端并发连接（使用多线程）
- ✅ Protobuf 消息协议（规范化、类型安全）
- ✅ 消息广播：用户发送的消息会广播给所有其他用户
- ✅ 用户上线/下线通知
- ✅ 设置名字功能（可选，替代 IP:Port 显示）
- ✅ 时间戳支持
- ✅ 线程安全的用户列表管理

## 学习重点

这个版本在 v0.3 基础上增加了 Protobuf 协议支持：
- Protobuf 协议定义和代码生成
- 消息序列化/反序列化
- 二进制协议传输（长度前缀）
- 名字验证和错误处理
- 消息类型规范化（SC_/CS_ 前缀）

## 版本历史

- **v0.1**: 单客户端 Echo Server
- **v0.2**: 多客户端 Echo Server（读写分离）
- **v0.3**: 聊天室服务器（消息广播，纯文本协议）
- **v0.4** (当前): Protobuf 协议规范化 + 设置名字

## 编译

### 前置要求

- Protobuf 编译器（protoc）
- Protobuf C++ 库
- CMake 3.10+

### 使用 CMake

```bash
mkdir build
cd build
cmake ..
make
```

### 手动编译

需要先编译 Protobuf 代码：

```bash
# 生成 Protobuf 代码
protoc --cpp_out=. ../proto/chat.proto

# 编译服务器
g++ -std=c++17 -pthread -I. -I/opt/homebrew/include \
    -L/opt/homebrew/lib -lprotobuf \
    echo_server.cpp chat.pb.cc -o echo_server
```

## 运行

```bash
./echo_server [端口号]
```

默认端口为 8080。例如：

```bash
# 使用默认端口 8080
./echo_server

# 使用自定义端口
./echo_server 3000
```

## 测试

### 使用多个客户端程序测试

```bash
# 终端1：启动服务器
cd server/build
./echo_server

# 终端2：启动客户端1
cd client/build
./echo_client

# 终端3：启动客户端2
cd client/build
./echo_client
```

在客户端中：
1. 可以设置名字：`/name Alice`
2. 发送消息：直接输入消息内容
3. 所有其他客户端都会收到广播

## 消息协议

### Protobuf 协议

使用 Protobuf 定义消息格式，所有消息都是二进制格式。

### 用户标识

- **内部标识**：`IP:Port`（唯一，不可变）
- **显示标识**：用户设置的名字（可选）
  - 如果设置了名字，消息显示使用名字
  - 如果未设置名字，消息显示使用 IP:Port

### 消息类型

**客户端 → 服务器**：
- `CS_SET_NAME`: 设置名字
- `CS_CHAT`: 发送聊天消息

**服务器 → 客户端**：
- `SC_SET_NAME_RESPONSE`: 设置名字响应
- `SC_BROADCAST`: 广播消息
- `SC_USER_JOINED`: 用户加入通知
- `SC_USER_LEFT`: 用户离开通知
- `SC_ERROR`: 错误消息

### 消息格式示例

**设置名字**：
```
客户端发送: CS_SetNameRequest { type: CS_SET_NAME, name: "Alice" }
服务器响应: SC_SetNameResponse { type: SC_SET_NAME_RESPONSE, success: true, new_name: "Alice" }
```

**聊天消息**：
```
客户端发送: ChatMessage { type: CS_CHAT, message: "Hello!" }
服务器广播: ChatMessage { type: SC_BROADCAST, from: "Alice", message: "Hello!", timestamp: 1234567890 }
```

## 架构说明

- **多线程模型**：每个客户端连接使用独立的线程处理
- **连接池管理**：使用 `std::vector<ClientInfo>` 维护在线用户列表
- **线程安全**：使用 `std::mutex` 保护共享资源
- **Protobuf 协议**：二进制消息格式，类型安全
- **消息传输**：使用长度前缀（4字节） + 消息内容

## 代码结构

- `proto/chat.proto`: Protobuf 协议定义
- `ClientInfo` 结构体：存储客户端信息（socket, user_id, display_name）
- `clients` 列表：维护所有在线用户
- `broadcast_protobuf_message()`: 广播 Protobuf 消息
- `handle_set_name()`: 处理设置名字请求
- `handle_chat()`: 处理聊天消息

## 工作流程

1. 客户端连接
2. 服务器自动生成用户标识（IP:Port）
3. 添加到在线用户列表
4. 通知所有其他用户：新用户加入（使用 Protobuf）
5. 客户端可以选择设置名字（可选）
6. 接收用户消息（Protobuf 格式）并广播给所有其他用户
7. 用户断开时，从列表移除并通知其他用户

## 下一步

- [ ] 实现分组/频道功能
- [ ] 添加私聊功能
- [ ] 支持 Web 客户端
