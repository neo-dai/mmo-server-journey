#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "chat.pb.h"

struct ClientInfo {
    int socket;
    std::string user_id;        // IP:Port 格式（内部ID）
    std::string display_name;   // 显示名字（可选）
    bool has_name;              // 是否设置了名字
    
    ClientInfo() : socket(-1), has_name(false) {}
    
    std::string get_display_id() const {
        return has_name ? display_name : user_id;
    }
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

    ClientInfo* find_client(int socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto& client : clients) {
            if (client.socket == socket) {
                return &client;
            }
        }
        return nullptr;
    }

    bool validate_name(const std::string& name) {
        if (name.empty() || name.length() > 20) {
            return false;
        }
        // 检查字符：字母、数字、下划线、中文
        for (char c : name) {
            if (!((c >= 'a' && c <= 'z') || 
                  (c >= 'A' && c <= 'Z') || 
                  (c >= '0' && c <= '9') || 
                  c == '_' || 
                  (c & 0x80))) {  // 中文字符
                return false;
            }
        }
        return true;
    }

    bool send_protobuf_message(int socket, const google::protobuf::Message& msg) {
        std::string data;
        if (!msg.SerializeToString(&data)) {
            return false;
        }
        
        // 发送消息长度（4字节，网络字节序）
        uint32_t len = htonl(data.length());
        ssize_t sent = send(socket, &len, sizeof(len), 0);
        if (sent != sizeof(len)) {
            return false;
        }
        
        // 发送消息内容
        sent = send(socket, data.c_str(), data.length(), 0);
        return sent == static_cast<ssize_t>(data.length());
    }

    bool recv_protobuf_message(int socket, std::string& data) {
        // 接收消息长度
        uint32_t len_net;
        ssize_t received = recv(socket, &len_net, sizeof(len_net), MSG_WAITALL);
        if (received != sizeof(len_net)) {
            return false;
        }
        
        uint32_t len = ntohl(len_net);
        if (len == 0 || len > 65536) {  // 限制最大消息长度
            return false;
        }
        
        // 接收消息内容
        data.resize(len);
        received = recv(socket, &data[0], len, MSG_WAITALL);
        return received == static_cast<ssize_t>(len);
    }

    void broadcast_protobuf_message(const google::protobuf::Message& msg, int exclude_socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        for (const auto& client : clients) {
            if (client.socket != exclude_socket) {
                send_protobuf_message(client.socket, msg);
            }
        }
    }

    void send_user_joined(int socket, const ClientInfo& client) {
        chat::ChatMessage msg;
        msg.set_type(chat::SC_USER_JOINED);
        msg.set_from(client.get_display_id());
        msg.set_user_id(client.user_id);
        msg.set_display_name(client.display_name);
        msg.set_timestamp(time(nullptr));
        
        broadcast_protobuf_message(msg, socket);
    }

    void send_user_left(const ClientInfo& client) {
        chat::ChatMessage msg;
        msg.set_type(chat::SC_USER_LEFT);
        msg.set_from(client.get_display_id());
        msg.set_user_id(client.user_id);
        msg.set_display_name(client.display_name);
        msg.set_timestamp(time(nullptr));
        
        broadcast_protobuf_message(msg, -1);
    }

    void handle_set_name(int socket, ClientInfo* client, const chat::CS_SetNameRequest& request) {
        std::string name = request.name();
        
        // 验证名字
        if (!validate_name(name)) {
            chat::SC_SetNameResponse response;
            response.set_type(chat::SC_SET_NAME_RESPONSE);
            response.set_success(false);
            response.set_message("名字无效：长度必须在1-20个字符之间，只能包含字母、数字、下划线和中文");
            send_protobuf_message(socket, response);
            
            chat::SC_ErrorMessage error;
            error.set_type(chat::SC_ERROR);
            error.set_error_code("INVALID_NAME");
            error.set_error_message("名字格式不正确");
            send_protobuf_message(socket, error);
            return;
        }
        
        // 更新名字
        std::string old_name = client->display_name;
        client->display_name = name;
        client->has_name = true;
        
        // 发送成功响应
        chat::SC_SetNameResponse response;
        response.set_type(chat::SC_SET_NAME_RESPONSE);
        response.set_success(true);
        response.set_message("名字设置成功");
        response.set_new_name(name);
        if (!old_name.empty()) {
            response.set_old_name(old_name);
        }
        send_protobuf_message(socket, response);
        
        log(client->user_id + " 设置名字: " + name);
    }

    void handle_chat(int socket, ClientInfo* client, const chat::ChatMessage& request) {
        std::string message = request.message();
        if (message.empty()) {
            return;
        }
        
        log("收到消息 [" + client->get_display_id() + "]: " + message);
        
        // 构建广播消息
        chat::ChatMessage broadcast;
        broadcast.set_type(chat::SC_BROADCAST);
        broadcast.set_from(client->get_display_id());
        broadcast.set_message(message);
        broadcast.set_timestamp(time(nullptr));
        broadcast.set_user_id(client->user_id);
        if (client->has_name) {
            broadcast.set_display_name(client->display_name);
        }
        
        broadcast_protobuf_message(broadcast, socket);
    }

    void add_client(int socket, const std::string& user_id) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        ClientInfo client;
        client.socket = socket;
        client.user_id = user_id;
        client.has_name = false;
        clients.push_back(client);
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

        // 创建客户端信息
        ClientInfo client_info;
        client_info.socket = client_socket;
        client_info.user_id = user_id;
        client_info.has_name = false;

        // 添加到客户端列表
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(client_info);
        }

        // 通知其他用户：新用户加入
        send_user_joined(client_socket, client_info);
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            log(user_id + " 加入聊天室，当前在线: " + std::to_string(clients.size()));
        }

        std::string data;
        
        while (true) {
            // 接收 Protobuf 消息
            if (!recv_protobuf_message(client_socket, data)) {
                log("客户端断开连接或接收错误: " + user_id);
                break;
            }

            // 先尝试解析为 CS_SetNameRequest（检查 type 字段）
            chat::CS_SetNameRequest set_name_request;
            if (set_name_request.ParseFromString(data) && 
                set_name_request.type() == chat::CS_SET_NAME && 
                !set_name_request.name().empty()) {
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    ClientInfo* client = find_client(client_socket);
                    if (client) {
                        handle_set_name(client_socket, client, set_name_request);
                    }
                }
                continue;
            }
            
            // 尝试解析为 ChatMessage (CS_CHAT)
            chat::ChatMessage msg;
            if (msg.ParseFromString(data) && msg.type() == chat::CS_CHAT) {
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    ClientInfo* client = find_client(client_socket);
                    if (client) {
                        handle_chat(client_socket, client, msg);
                    }
                }
                continue;
            }
            
            log("解析消息失败或未知消息类型: " + user_id);
        }

        // 从客户端列表移除前，获取客户端信息用于通知
        ClientInfo client_to_notify;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            ClientInfo* client = find_client(client_socket);
            if (client) {
                client_to_notify = *client;
            } else {
                client_to_notify = client_info;  // 使用初始信息
            }
            remove_client(client_socket);
        }
        
        // 通知其他用户：用户离开
        send_user_left(client_to_notify);

        close(client_socket);
        
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
