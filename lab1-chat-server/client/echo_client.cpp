#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

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

    // 1. 创建 socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        std::cerr << "创建 socket 失败" << std::endl;
        return 1;
    }

    // 2. 设置服务器地址
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "无效的服务器地址: " << server_ip << std::endl;
        close(client_socket);
        return 1;
    }

    // 3. 连接到服务器
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "连接服务器失败: " << server_ip << ":" << server_port << std::endl;
        close(client_socket);
        return 1;
    }

    std::cout << "[客户端] 已连接到服务器: " << server_ip << ":" << server_port << std::endl;
    std::cout << "=== Echo Client ===" << std::endl;
    std::cout << "输入消息发送到服务器（输入 'quit' 或 'exit' 退出）" << std::endl;
    std::cout << "========================================" << std::endl;

    // 4. 循环：发送消息 -> 等待接收回显
    char buffer[4096] = {0};
    std::string input;

    while (true) {
        // 获取用户输入
        std::getline(std::cin, input);

        if (input.empty()) {
            continue;
        }

        // 检查退出命令
        if (input == "quit" || input == "exit") {
            break;
        }

        // 发送消息
        ssize_t bytes_sent = send(client_socket, input.c_str(), input.length(), 0);
        if (bytes_sent < 0) {
            std::cerr << "[客户端] 发送消息失败" << std::endl;
            break;
        }

        std::cout << "[客户端] 已发送: " << input << std::endl;

        // 等待接收回显
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                std::cout << "[客户端] 服务器断开连接" << std::endl;
            } else {
                std::cerr << "[客户端] 接收数据错误" << std::endl;
            }
            break;
        }

        buffer[bytes_read] = '\0';
        std::cout << "[客户端] 收到回显: " << buffer << std::endl;
    }

    // 5. 关闭连接
    close(client_socket);
    std::cout << "[客户端] 已断开连接" << std::endl;
    
    return 0;
}
