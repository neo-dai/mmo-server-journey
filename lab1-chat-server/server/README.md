# Echo Server (C++)

一个支持多客户端并发的 C++ Echo 服务器。

## 功能特性

- ✅ TCP Socket 服务器
- ✅ 多客户端并发连接（使用多线程）
- ✅ Echo 功能：服务器原样返回客户端发送的消息
- ✅ 客户端连接/断开日志
- ✅ 线程安全的日志输出

## 学习重点

这个版本在 v0.1 基础上增加了多线程支持：
- Socket 创建和配置
- 地址绑定和端口监听
- 多客户端连接处理
- 多线程编程（每个客户端独立线程）
- 线程同步（mutex 保护共享资源）
- 数据收发（send/recv）

## 版本历史

- **v0.1**: 单客户端版本，适合学习 Socket 编程基础
- **v0.2** (当前): 多客户端版本，支持并发连接

## 编译

### 使用 CMake

```bash
mkdir build
cd build
cmake ..
make
```

### 手动编译

```bash
g++ -std=c++11 -pthread -o echo_server echo_server.cpp
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

### 使用 telnet 测试

```bash
# 可以同时打开多个终端连接
telnet localhost 8080
```

连接后，输入任何消息，服务器会原样返回。

### 使用 nc (netcat) 测试

```bash
nc localhost 8080
```

### 使用 Python 客户端测试

```python
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('localhost', 8080))

# 发送消息
s.send(b'Hello, Server!')

# 接收回显
response = s.recv(1024)
print('收到:', response.decode())

s.close()
```

## 消息协议

当前版本支持纯文本消息。客户端发送的任何文本消息都会被服务器原样返回。

后续版本将支持 JSON 格式的消息协议，例如：

```json
{"type": "test", "message": "hello"}
```

## 架构说明

- **多线程模型**：每个客户端连接使用独立的线程处理
- **线程安全**：使用 `std::mutex` 保护日志输出
- **并发支持**：可以同时处理多个客户端连接
- **资源管理**：使用 RAII 原则管理 socket 资源

## 代码结构

代码采用面向对象设计：
- `EchoServer` 类：封装服务器逻辑
- `handle_client()` 方法：在独立线程中处理每个客户端
- 线程池管理：使用 `std::vector<std::thread>` 管理客户端线程

## 下一步

- [ ] 添加 JSON 消息协议支持
- [ ] 实现消息解析和验证
- [ ] 添加错误处理和异常处理
- [ ] 支持优雅关闭（信号处理）
- [ ] 性能优化（连接池、消息队列等）
