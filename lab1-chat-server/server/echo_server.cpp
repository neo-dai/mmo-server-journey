#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    int port = 8080;
    
    // 解析命令行参数
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    // 1. 创建 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "创建 socket 失败" << std::endl;
        return 1;
    }

    // 2. 绑定地址和端口
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "绑定端口 " << port << " 失败" << std::endl;
        close(server_fd);
        return 1;
    }

    // 3. 开始监听
    if (listen(server_fd, 1) < 0) {
        std::cerr << "监听失败" << std::endl;
        close(server_fd);
        return 1;
    }

    std::cout << "[服务器] 启动成功，监听端口: " << port << std::endl;
    std::cout << "[服务器] 等待客户端连接..." << std::endl;

    // 4. 接受客户端连接（只接受一个客户端）
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    
    if (client_socket < 0) {
        std::cerr << "[服务器] 接受连接失败" << std::endl;
        close(server_fd);
        return 1;
    }

    // 显示客户端信息
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    std::cout << "[服务器] 客户端已连接: " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

    // 5. 处理消息：接收 -> 回显
    char buffer[4096] = {0};
    
    while (true) {
        // 接收数据
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                std::cout << "[服务器] 客户端断开连接" << std::endl;
            } else {
                std::cerr << "[服务器] 接收数据错误" << std::endl;
            }
            break;
        }

        buffer[bytes_read] = '\0';
        std::cout << "[服务器] 收到消息: " << buffer << std::endl;

        // Echo: 原样返回消息
        ssize_t bytes_sent = send(client_socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            std::cerr << "[服务器] 发送数据错误" << std::endl;
            break;
        }
        
        std::cout << "[服务器] 已回显消息" << std::endl;
    }

    // 6. 关闭连接
    close(client_socket);
    close(server_fd);
    
    std::cout << "[服务器] 服务器已关闭" << std::endl;
    return 0;
}
