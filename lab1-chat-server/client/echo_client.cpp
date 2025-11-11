#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class ChatClient {
private:
    int client_socket;
    std::string server_ip;
    int server_port;
    bool connected;
    std::mutex cout_mutex;

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << message;
    }

    void receive_messages() {
        char buffer[4096] = {0};
        
        while (connected) {
            ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    log("[系统] 服务器断开连接\n");
                } else {
                    log("[系统] 接收数据错误\n");
                }
                connected = false;
                break;
            }

            buffer[bytes_read] = '\0';
            std::string message(buffer);
            
            // 直接显示服务器发送的消息（已包含格式）
            log(message);
        }
    }

public:
    ChatClient(const std::string& ip, int port) 
        : client_socket(-1), server_ip(ip), server_port(port), connected(false) {}

    ~ChatClient() {
        disconnect();
    }

    bool connect() {
        // 创建 socket
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket < 0) {
            std::cerr << "创建 socket 失败" << std::endl;
            return false;
        }

        // 设置服务器地址
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "无效的服务器地址: " << server_ip << std::endl;
            close(client_socket);
            return false;
        }

        // 连接到服务器
        if (::connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "连接服务器失败: " << server_ip << ":" << server_port << std::endl;
            close(client_socket);
            return false;
        }

        connected = true;
        log("[客户端] 已连接到服务器: " + server_ip + ":" + std::to_string(server_port) + "\n");

        // 启动接收消息的线程（读写分离）
        std::thread recv_thread(&ChatClient::receive_messages, this);
        recv_thread.detach();

        return true;
    }

    bool send_message(const std::string& message) {
        if (!connected) {
            std::cerr << "未连接到服务器" << std::endl;
            return false;
        }

        // 发送消息（添加换行符）
        std::string msg_with_newline = message + "\n";
        ssize_t bytes_sent = send(client_socket, msg_with_newline.c_str(), msg_with_newline.length(), 0);
        if (bytes_sent < 0) {
            log("[系统] 发送消息失败\n");
            connected = false;
            return false;
        }

        return true;
    }

    void disconnect() {
        if (client_socket >= 0) {
            connected = false;
            close(client_socket);
            client_socket = -1;
        }
    }

    bool is_connected() const {
        return connected;
    }
};

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;

    // 解析命令行参数
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = std::stoi(argv[2]);
    }

    ChatClient client(server_ip, server_port);

    if (!client.connect()) {
        return 1;
    }

    std::cout << "=== 聊天室客户端 ===" << std::endl;
    std::cout << "输入消息发送到聊天室（输入 'quit' 或 'exit' 退出）" << std::endl;
    std::cout << "========================================" << std::endl;

    std::string input;
    while (client.is_connected()) {
        std::getline(std::cin, input);

        if (input.empty()) {
            continue;
        }

        // 检查退出命令
        if (input == "quit" || input == "exit") {
            break;
        }

        // 发送消息
        if (!client.send_message(input)) {
            break;
        }
    }

    client.disconnect();
    return 0;
}
