#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "chat.pb.h"

class ChatClient {
private:
    int client_socket;
    std::string server_ip;
    int server_port;
    bool connected;
    std::string user_id;        // 从服务器接收的 IP:Port
    std::string display_name;   // 用户设置的名字
    bool name_set;              // 是否已设置名字
    std::mutex cout_mutex;

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << message;
    }

    bool send_protobuf_message(const google::protobuf::Message& msg) {
        std::string data;
        if (!msg.SerializeToString(&data)) {
            return false;
        }
        
        // 发送消息长度（4字节，网络字节序）
        uint32_t len = htonl(data.length());
        ssize_t sent = send(client_socket, &len, sizeof(len), 0);
        if (sent != sizeof(len)) {
            return false;
        }
        
        // 发送消息内容
        sent = send(client_socket, data.c_str(), data.length(), 0);
        return sent == static_cast<ssize_t>(data.length());
    }

    bool recv_protobuf_message(std::string& data) {
        // 接收消息长度
        uint32_t len_net;
        ssize_t received = recv(client_socket, &len_net, sizeof(len_net), MSG_WAITALL);
        if (received != sizeof(len_net)) {
            return false;
        }
        
        uint32_t len = ntohl(len_net);
        if (len == 0 || len > 65536) {
            return false;
        }
        
        // 接收消息内容
        data.resize(len);
        received = recv(client_socket, &data[0], len, MSG_WAITALL);
        return received == static_cast<ssize_t>(len);
    }

    void display_message(const chat::ChatMessage& msg) {
        chat::MessageType type = msg.type();
        
        if (type == chat::SC_BROADCAST) {
            log("[" + msg.from() + "] " + msg.message() + "\n");
        } else if (type == chat::SC_USER_JOINED) {
            log("[系统] " + msg.from() + " 加入了聊天室\n");
        } else if (type == chat::SC_USER_LEFT) {
            log("[系统] " + msg.from() + " 离开了聊天室\n");
        }
    }

    void receive_messages() {
        std::string data;
        
        while (connected) {
            if (!recv_protobuf_message(data)) {
                if (connected) {
                    log("[系统] 服务器断开连接\n");
                }
                connected = false;
                break;
            }

            // 尝试解析不同类型的消息
            bool parsed = false;
            
            // 尝试解析为 SC_SetNameResponse
            chat::SC_SetNameResponse response;
            if (response.ParseFromString(data) && response.type() == chat::SC_SET_NAME_RESPONSE) {
                if (response.success()) {
                    display_name = response.new_name();
                    name_set = true;
                    log("[系统] 名字设置成功: " + display_name + "\n");
                } else {
                    log("[系统] 名字设置失败: " + response.message() + "\n");
                }
                parsed = true;
            }
            
            // 尝试解析为 SC_ErrorMessage
            if (!parsed) {
                chat::SC_ErrorMessage error;
                if (error.ParseFromString(data) && error.type() == chat::SC_ERROR) {
                    log("[错误] " + error.error_message() + "\n");
                    parsed = true;
                }
            }
            
            // 尝试解析为 ChatMessage
            if (!parsed) {
                chat::ChatMessage msg;
                if (msg.ParseFromString(data)) {
                    chat::MessageType type = msg.type();
                    
                    if (type == chat::SC_USER_JOINED) {
                        // 如果是自己加入，保存 user_id
                        if (user_id.empty() && !msg.user_id().empty()) {
                            user_id = msg.user_id();
                        }
                        display_message(msg);
                    } else if (type == chat::SC_BROADCAST || type == chat::SC_USER_LEFT) {
                        display_message(msg);
                    }
                    parsed = true;
                }
            }
            
            if (!parsed) {
                log("[系统] 解析消息失败\n");
            }
        }
    }

public:
    ChatClient(const std::string& ip, int port) 
        : client_socket(-1), server_ip(ip), server_port(port), 
          connected(false), name_set(false) {}

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

        // 启动接收消息的线程
        std::thread recv_thread(&ChatClient::receive_messages, this);
        recv_thread.detach();

        return true;
    }

    bool set_name(const std::string& name) {
        if (!connected) {
            std::cerr << "未连接到服务器" << std::endl;
            return false;
        }

        chat::CS_SetNameRequest request;
        request.set_type(chat::CS_SET_NAME);
        request.set_name(name);
        
        return send_protobuf_message(request);
    }

    bool send_chat_message(const std::string& message) {
        if (!connected) {
            std::cerr << "未连接到服务器" << std::endl;
            return false;
        }

        chat::ChatMessage msg;
        msg.set_type(chat::CS_CHAT);
        msg.set_message(message);
        msg.set_timestamp(time(nullptr));
        
        return send_protobuf_message(msg);
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

    bool has_name() const {
        return name_set;
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
    std::cout << "输入消息发送到聊天室" << std::endl;
    std::cout << "输入 '/name <名字>' 设置名字（可选）" << std::endl;
    std::cout << "输入 'quit' 或 'exit' 退出" << std::endl;
    std::cout << "========================================" << std::endl;

    // 等待一下，让接收线程初始化
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

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

        // 检查设置名字命令
        if (input.length() > 6 && input.substr(0, 6) == "/name ") {
            std::string name = input.substr(6);
            if (!name.empty()) {
                client.set_name(name);
            } else {
                std::cout << "[系统] 名字不能为空" << std::endl;
            }
            continue;
        }

        // 发送聊天消息
        if (!client.send_chat_message(input)) {
            break;
        }
    }

    client.disconnect();
    return 0;
}
