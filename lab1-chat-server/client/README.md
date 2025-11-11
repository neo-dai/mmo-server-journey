# Chat Client (C++)

一个支持聊天室的 C++ 客户端，使用 Protobuf 协议与服务器通信。

## 功能特性

- ✅ TCP Socket 客户端
- ✅ 连接到聊天服务器
- ✅ Protobuf 消息协议（规范化、类型安全）
- ✅ 交互式消息发送
- ✅ 自动接收广播消息（多线程）
- ✅ 设置名字功能（可选）
- ✅ 显示用户上线/下线通知
- ✅ 支持自定义服务器地址和端口

## 学习重点

这个版本在 v0.3 基础上增加了 Protobuf 协议支持：
- Protobuf 消息序列化/反序列化
- 二进制协议传输（长度前缀）
- 消息类型处理（SC_/CS_ 前缀）
- 名字设置和显示

## 版本历史

- **v0.1**: 请求-响应模式 Echo Client
- **v0.2**: 读写分离 Echo Client（双全工）
- **v0.3**: 聊天室客户端（消息广播，纯文本协议）
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

# 编译客户端
g++ -std=c++17 -pthread -I. -I/opt/homebrew/include \
    -L/opt/homebrew/lib -lprotobuf \
    echo_client.cpp chat.pb.cc -o echo_client
```

## 运行

```bash
./echo_client [服务器IP] [端口号]
```

参数说明：
- `服务器IP`：服务器地址，默认为 `127.0.0.1`
- `端口号`：服务器端口，默认为 `8080`

示例：

```bash
# 使用默认地址和端口（127.0.0.1:8080）
./echo_client

# 指定服务器地址
./echo_client 192.168.1.100

# 指定服务器地址和端口
./echo_client 192.168.1.100 3000
```

## 使用方法

1. 启动客户端后，自动连接到服务器
2. 连接后自动加入聊天室（使用 IP:Port 作为标识）
3. **可选**：设置名字（输入 `/name <名字>`）
4. 输入消息并按回车发送
5. 所有其他用户会收到你的消息
6. 你会收到其他用户的消息和系统通知
7. 输入 `quit` 或 `exit` 退出客户端

示例会话：

```
=== 聊天室客户端 ===
输入消息发送到聊天室
输入 '/name <名字>' 设置名字（可选）
输入 'quit' 或 'exit' 退出
========================================
[客户端] 已连接到服务器: 127.0.0.1:8080
[系统] 127.0.0.1:54322 加入了聊天室
/name Alice
[系统] 名字设置成功: Alice
Hello everyone!
[Alice] Hello everyone!
[Bob] Hi there!
quit
```

## 测试流程

1. **启动服务器**（在另一个终端）：
   ```bash
   cd ../server/build
   ./echo_server
   ```

2. **启动多个客户端**（在不同终端）：
   ```bash
   cd ../client/build
   ./echo_client
   ```

3. **测试功能**：
   - 在客户端1设置名字：`/name Alice`
   - 在客户端2设置名字：`/name Bob`
   - 在客户端1发送消息，观察客户端2是否收到
   - 测试名字验证（尝试设置无效名字）

## 消息协议

### Protobuf 协议

使用 Protobuf 定义消息格式，所有消息都是二进制格式。

### 命令

- `/name <名字>`: 设置显示名字
  - 名字长度：1-20 个字符
  - 允许字符：字母、数字、下划线、中文
  - 如果设置成功，后续消息显示使用名字而非 IP:Port

### 消息类型

**客户端发送**：
- `CS_SET_NAME`: 设置名字请求
- `CS_CHAT`: 聊天消息

**客户端接收**：
- `SC_SET_NAME_RESPONSE`: 设置名字响应
- `SC_BROADCAST`: 广播消息
- `SC_USER_JOINED`: 用户加入通知
- `SC_USER_LEFT`: 用户离开通知
- `SC_ERROR`: 错误消息

## 架构说明

- **多线程模型**：使用独立线程接收服务器消息，主线程处理用户输入
- **读写分离**：发送和接收操作可以同时进行
- **线程安全**：使用 `std::mutex` 保护日志输出
- **Protobuf 协议**：二进制消息格式，类型安全
- **消息传输**：使用长度前缀（4字节） + 消息内容

## 代码结构

- `proto/chat.proto`: Protobuf 协议定义
- `ChatClient` 类：封装客户端逻辑
- `receive_messages()` 方法：在独立线程中接收服务器消息
- `set_name()` 方法：发送设置名字请求
- `send_chat_message()` 方法：发送聊天消息

## 与 v0.3 的区别

| 特性 | v0.3 | v0.4 |
|------|------|------|
| 消息协议 | 纯文本 | Protobuf（二进制） |
| 用户标识 | IP:Port | IP:Port + 可选名字 |
| 消息格式 | 简单文本 | 结构化 Protobuf |
| 类型安全 | 无 | 有 |
| 扩展性 | 较差 | 好 |

## 下一步

- [ ] 实现分组/频道功能
- [ ] 添加私聊功能
- [ ] 支持 Web 客户端
