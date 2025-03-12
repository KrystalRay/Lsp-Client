#include "lsp_client.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>   
#include <chrono>     

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return content;
}

int main() {
    // 创建LSP客户端
    LSPClient client;
    
    // 初始化连接到MagpieBridge服务器
    // 注意：这里需要指定你的JAR文件路径
#ifdef _WIN32
    std::string serverPath = "java -jar d:\\课程\\大四\\magpiebridge\\target\\HelloWorld-0.0.2-SNAPSHOT.jar";
#else
    std::string serverPath = "/mnt/d/课程/大四/magpiebridge/target/HelloWorld-0.0.2-SNAPSHOT.jar";
#endif
    
    if (!client.initialize(serverPath)) {
        std::cerr << "Failed to initialize LSP client" << std::endl;
        return 1;
    }
    
    // 打开一个Java文件进行分析
#ifdef _WIN32
    std::string filePath = "d:\\课程\\大四\\magpiebridge\\src\\main\\java\\org\\example\\HelloWorld.java";
#else
    std::string filePath = "/mnt/d/课程/大四/magpiebridge/src/main/java/org/example/HelloWorld.java";
#endif
    std::string fileContent = readFile(filePath);
    
#ifdef _WIN32
    std::string fileUri = "file:///" + filePath;
    // 替换路径中的反斜杠为正斜杠
    std::replace(fileUri.begin(), fileUri.end(), '\\', '/');
#else
    // Linux下需要确保URI格式正确
    std::string fileUri = "file://" + filePath;
    // 确保没有多余的斜杠
    size_t pos = fileUri.find("////");
    if (pos != std::string::npos) {
        fileUri.replace(pos, 4, "///");
    }
#endif

    // 输出URI用于调试
    std::cout << "Using URI: " << fileUri << std::endl;
    
    client.openDocument(fileUri, fileContent, "java");
    
    // 等待诊断结果
    std::cout << "等待分析结果..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 获取诊断结果
    auto diagnostics = client.getDiagnostics(fileUri);
    std::cout << "收到 " << diagnostics.size() << " 条诊断信息" << std::endl;
    
    for (const auto& diag : diagnostics) {
        std::cout << "诊断: " << diag.dump(2) << std::endl;
    }
    
    // 关闭连接
    client.shutdown();
    
    return 0;
}