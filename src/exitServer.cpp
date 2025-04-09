#include "lsp_client.h"
#include <iostream>
#include <chrono>

int main() {
    // 创建LSP客户端
    LSPClient client;
    
    // 连接到已运行的服务器
    std::string serverHost = "localhost"; 
    int serverPort = 9090;
    
    std::cout << "正在尝试连接到服务器: " << serverHost << ":" << serverPort << std::endl;
    
    if (!client.connectToServer(serverHost, serverPort)) {
        std::cerr << "Failed to connect to LSP server" << std::endl;
        return 1;
    }
    
    std::cout << "成功连接到服务器，准备关闭..." << std::endl;
    
    // 调用exitServer方法关闭服务器
    client.exitServer();
    
    std::cout << "服务器已关闭" << std::endl;
    
    return 0;
}