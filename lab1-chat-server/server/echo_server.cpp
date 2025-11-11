#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class EchoServer {
private:
    int server_fd;
    int port;
    std::vector<std::thread> client_threads;
    std::mutex cout_mutex;

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[服务器] " << message << std::endl;
    }

    void handle_client(int client_socket, struct sockaddr_in client_addr) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        log("新客户端连接: " + std::string(client_ip) + ":" + std::to_string(ntohs(client_addr.sin_port)));

        char buffer[4096] = {0};
        
        while (true) {
            // 接收数据
            ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    log("客户端断开连接: " + std::string(client_ip));
                } else {
                    log("接收数据错误: " + std::string(client_ip));
                }
                break;
            }

            buffer[bytes_read] = '\0';
            std::string message(buffer);
            
            log("收到消息 [" + std::string(client_ip) + "]: " + message);

            // Echo: 原样返回消息
            ssize_t bytes_sent = send(client_socket, buffer, bytes_read, 0);
            if (bytes_sent < 0) {
                log("发送数据错误: " + std::string(client_ip));
                break;
            }
            
            log("已回显消息 [" + std::string(client_ip) + "]");
        }

        close(client_socket);
    }

public:
    EchoServer(int port) : server_fd(-1), port(port) {}

    ~EchoServer() {
        stop();
    }

    bool start() {
        // 创建 socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "创建 socket 失败" << std::endl;
            return false;
        }

        // 设置 socket 选项，允许地址重用
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "设置 socket 选项失败" << std::endl;
            close(server_fd);
            return false;
        }

        // 绑定地址和端口
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "绑定端口 " << port << " 失败" << std::endl;
            close(server_fd);
            return false;
        }

        // 开始监听（支持多个客户端连接）
        if (listen(server_fd, 10) < 0) {
            std::cerr << "监听失败" << std::endl;
            close(server_fd);
            return false;
        }

        log("服务器启动成功，监听端口: " + std::to_string(port));
        log("等待客户端连接...");

        return true;
    }

    void run() {
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            
            // 接受客户端连接
            int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
            
            if (client_socket < 0) {
                log("接受连接失败");
                continue;
            }

            // 为每个客户端创建新线程
            client_threads.emplace_back([this, client_socket, client_addr]() {
                handle_client(client_socket, client_addr);
            });
        }
    }

    void stop() {
        if (server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }

        // 等待所有客户端线程结束
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        client_threads.clear();
    }
};

int main(int argc, char* argv[]) {
    int port = 8080;
    
    // 解析命令行参数
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    EchoServer server(port);
    
    if (!server.start()) {
        return 1;
    }

    // 运行服务器（Ctrl+C 退出）
    try {
        server.run();
    } catch (...) {
        server.stop();
        return 1;
    }

    return 0;
}
