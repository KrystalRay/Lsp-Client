#include "lsp_client.h"
#include "file_utils.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <thread>   
#include <chrono>     
#include <unistd.h>

int main() {
    // 创建LSP客户端
    LSPClient client;
    
    // 初始化连接到MagpieBridge服务器
    std::string serverPath = "/mnt/d/Course/Year4/QLextension/target/HelloWorld-0.0.2-SNAPSHOT.jar";
    
    if (!client.initialize(serverPath)) {
        std::cerr << "Failed to initialize LSP client" << std::endl;
        return 1;
    }
    
    // 打开一个Java文件进行分析
    std::string filePath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src/main/java/tutorial1/Main.java";
    std::string srcPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src/main/java";
    std::string rootPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master";

    // 使用标准URI转换
    std::string fileUri = FileUtils::pathToUri(filePath);
    std::string rootUri = FileUtils::pathToUri(rootPath);
    // 读取文件内容
    std::string fileContent = FileUtils::readFile(filePath);
    
    json configParams = {
        {"settings", {
            {"java", {
                // 将sourcePaths提升到java层级（关键修复）
                {"sourcePaths", json::array({
                    FileUtils::pathToUri(srcPath)
                })},
                {"libraryPaths", json::array()},
                {"project", {
                    {"name", "DemoProject"},
                    {"type", "maven"},
                    {"dependencies", json::array()},
                    // 添加构建文件绝对路径
                    {"buildFile", FileUtils::pathToUri(rootPath + "/pom.xml")}
                }},
                {"analysis", {
                    {"level", "full"},
                    {"target", "java8"},
                    // 添加依赖解析配置
                    {"dependencyResolution", {
                        {"mode", "hybrid"},
                        {"mavenHome", "C:/Users/KrystalRay/.m2"} // 需替换为实际路径
                    }}
                }}
            }}
        }}
    };


    
    // 在初始化后添加Maven项目验证
    std::string mvnCmd = "mvn -f \"" + rootPath + "/pom.xml\" validate";
    std::system(mvnCmd.c_str());


    
    // 等待一段时间，确保服务器处理配置
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 发送配置参数
    client.sendNotification("workspace/didChangeConfiguration", configParams);

    // [新增] 改为通过日志验证配置
    std::cout << "当前配置参数:\n" << configParams.dump(2) << std::endl;

    // 打开文档并等待分析
    client.openDocument(fileUri, fileContent, "java");
    
    // [新增] 增加服务器日志监听
    std::cout << "开始监听服务器日志..." << std::endl;
    
    // 等待诊断结果
    std::cout << "等待分析结果..." << std::endl;
    
    // 创建一个循环来读取服务器消息
    int maxWaitTime = 15; // 增加等待时间到15秒
    bool analysisStarted = false;
    bool analysisFinished = false;
    
    for (int i = 0; i < maxWaitTime; i++) {
        // 尝试读取服务器消息
        json message = client.readMessage(100); // 非阻塞读取，超时100毫秒
        
        if (!message.empty()) {
            // 输出完整消息用于调试
            std::cout << "服务器原始消息: " << message.dump(2) << std::endl;
            
            // 处理不同类型的消息
            if (message.contains("method")) {
                std::string method = message["method"];
                std::cout << "收到服务器消息: " << method << std::endl;
                
                // 处理诊断信息
                if (method == "textDocument/publishDiagnostics" && message.contains("params")) {
                    auto params = message["params"];
                    if (params.contains("uri") && params.contains("diagnostics")) {
                        std::string uri = params["uri"];
                        client.updateDiagnostics(uri, params["diagnostics"]);
                        std::cout << "更新了诊断信息，URI: " << uri << std::endl;
                        std::cout << "诊断内容: " << params["diagnostics"].dump(2) << std::endl;
                    }
                }
                // 处理其他类型的消息
                else if (method == "window/showMessage" && message.contains("params")) {
                    auto params = message["params"];
                    if (params.contains("message")) {
                        std::string msg = params["message"];
                        std::cout << "服务器消息: " << msg << std::endl;
                        
                        // 检测分析开始和结束
                        if (msg.find("started analyzing") != std::string::npos) {
                            analysisStarted = true;
                        }
                        if (msg.find("finished analyzing") != std::string::npos) {
                            analysisFinished = true;
                        }
                    }
                }
            }
        }
        
        // 检查是否已经收到诊断信息
        auto diagnostics = client.getDiagnostics(fileUri);
        if (!diagnostics.empty()) {
            std::cout << "已收到诊断信息，停止等待" << std::endl;
            break;
        }
        
        // 如果分析已完成但没有诊断信息，可以提前退出
        if (analysisStarted && analysisFinished) {
            // std::cout << "分析已完成，但没有诊断信息" << std::endl;
            // 再等待1秒，以防诊断信息延迟到达
            std::this_thread::sleep_for(std::chrono::seconds(1));
            break;
        }
    }
    std::cout << "Finish using WALA Analysis" << std::endl;
    // 获取诊断结果
    auto diagnostics = client.getDiagnostics(fileUri);
    std::cout << "收到 " << diagnostics.size() << " 条诊断信息" << std::endl;
    
    for (const auto& diag : diagnostics) {
        std::cout << "诊断: " << diag.dump(2) << std::endl;
    }
     
    // 测试1：获取分析器名称
    json sourceName = client.requestSourceName();
    std::cout << "分析器名称: " << sourceName.dump(2) << std::endl;

    // 关闭连接
    client.shutdown();
    
    return 0;
}