#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

struct ClientInfo {
    int socket;
    std::string user_id;  // IP:Port 格式
};

class ChatServer {
private:
    int server_fd;
    int port;
    std::vector<ClientInfo> clients;
    std::vector<std::thread> client_threads;
    std::mutex clients_mutex;
    std::mutex cout_mutex;

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[服务器] " << message << std::endl;
    }

    std::string get_user_id(struct sockaddr_in addr) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
        return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
    }

    void broadcast_message(const std::string& message, int exclude_socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        for (const auto& client : clients) {
            if (client.socket != exclude_socket) {
                ssize_t bytes_sent = send(client.socket, message.c_str(), message.length(), 0);
                if (bytes_sent < 0) {
                    // 发送失败，忽略（会在断开时处理）
                }
            }
        }
    }

    void add_client(int socket, const std::string& user_id) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back({socket, user_id});
    }

    void remove_client(int socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(
            std::remove_if(clients.begin(), clients.end(),
                [socket](const ClientInfo& c) { return c.socket == socket; }),
            clients.end()
        );
    }

    void handle_client(int client_socket, struct sockaddr_in client_addr) {
        std::string user_id = get_user_id(client_addr);
        log("新客户端连接: " + user_id);

        // 添加到客户端列表
        add_client(client_socket, user_id);

        // 通知其他用户：新用户加入
        std::string join_msg = "[系统] " + user_id + " 加入了聊天室\n";
        broadcast_message(join_msg, client_socket);
        
        // 获取在线人数（需要加锁）
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            log(user_id + " 加入聊天室，当前在线: " + std::to_string(clients.size()));
        }

        char buffer[4096] = {0};
        
        while (true) {
            // 接收数据
            ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    log("客户端断开连接: " + user_id);
                } else {
                    log("接收数据错误: " + user_id);
                }
                break;
            }

            buffer[bytes_read] = '\0';
            std::string message(buffer);
            
            // 移除末尾的换行符（如果有）
            if (!message.empty() && message.back() == '\n') {
                message.pop_back();
            }
            if (!message.empty() && message.back() == '\r') {
                message.pop_back();
            }

            if (message.empty()) {
                continue;
            }

            log("收到消息 [" + user_id + "]: " + message);

            // 广播消息给所有其他用户
            std::string broadcast_msg = "[" + user_id + "] " + message + "\n";
            broadcast_message(broadcast_msg, client_socket);
        }

        // 从客户端列表移除
        remove_client(client_socket);

        // 通知其他用户：用户离开
        std::string leave_msg = "[系统] " + user_id + " 离开了聊天室\n";
        broadcast_message(leave_msg, -1);

        close(client_socket);
        
        // 获取在线人数（需要加锁）
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            log(user_id + " 已断开，当前在线: " + std::to_string(clients.size()));
        }
    }

public:
    ChatServer(int port) : server_fd(-1), port(port) {}

    ~ChatServer() {
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

    ChatServer server(port);
    
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
