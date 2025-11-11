# Echo Server (C++)

一个简化的 C++ Echo 服务器，适合新手学习 Socket 编程基础。

## 功能特性

- ✅ TCP Socket 服务器
- ✅ 单客户端连接（简化版本，适合学习）
- ✅ Echo 功能：服务器原样返回客户端发送的消息
- ✅ 客户端连接/断开日志

## 学习重点

这个简化版本专注于 Socket 编程的基础知识：
- Socket 创建和配置
- 地址绑定和端口监听
- 接受客户端连接
- 数据收发（send/recv）

**注意**：这是简化版本，只支持单客户端连接。多线程和多客户端支持将在后续版本中学习。

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
g++ -std=c++11 -o echo_server echo_server.cpp
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

### 使用客户端程序测试

```bash
# 在另一个终端运行客户端
cd ../client/build
./echo_client
```

### 使用 telnet 测试

```bash
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

- **单线程模型**：服务器在主线程中处理客户端连接和消息
- **单客户端支持**：同时只支持一个客户端连接
- **请求-响应模式**：客户端发送消息后，等待服务器回显

## 代码结构

代码采用简单的函数式结构，易于理解：
1. 创建 socket
2. 绑定端口
3. 监听连接
4. 接受客户端连接
5. 循环处理消息（接收 -> 回显）
6. 关闭连接

## 下一步

- [ ] 添加多客户端支持（多线程版本）
- [ ] 添加 JSON 消息协议支持
- [ ] 实现消息解析和验证
- [ ] 添加错误处理和异常处理
- [ ] 支持优雅关闭（信号处理）
