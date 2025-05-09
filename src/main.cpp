#include "lsp_client.h"
#include "file_utils.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <thread>   
#include <chrono>     
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctime> // 新增头文件

int main() {
    // 创建LSP客户端
    LSPClient client;
    
    // 连接到已运行的服务器
    std::string serverHost = "172.19.112.1"; 
    int serverPort = 8080;
    
    std::cout << "正在尝试连接到服务器: " << serverHost << ":" << serverPort << std::endl;
    
    if (!client.connectToServer(serverHost, serverPort)) {
        std::cerr << "Failed to connect to LSP server" << std::endl;
        return 1;
    }
    std::cout << "成功连接到服务器" << std::endl;
    
    // 打开一个Java文件进行分析
    std::string filePath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src/main/java/tutorial1/Main.java";
    std::string srcPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src";
    std::string rootPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master";

    // 使用标准URI转换
    std::string fileUri = FileUtils::pathToUri(filePath);
    std::string rootUri = FileUtils::pathToUri(rootPath);
    // 读取文件内容
    std::string fileContent = FileUtils::readFile(filePath);
    
    // 发送工作区配置
    client.sendNotification("workspace/didChangeConfiguration", {
        {"settings", {
            {"java", {
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
                        {"mavenHome", "/root/.m2"} // 使用Linux路径
                    }}
                }}
            }}
        }}
    });

    // 记录发送wukong配置的起始时间
    auto analysisStart = std::chrono::steady_clock::now();

    // 在连接成功后发送配置
    nlohmann::json config = {
        {"wukong", {
            {"classpath", "/app/tests/qltests/java/advennet/"},
            // {"targetPath", "D:/Course/Year4/BS/examples/test/cwe089/semmle/tests"},
            // {"sourcesinkPath", "D:/Course/Year4/QLextension/Json/CWE-089-hiveMetaData.json"}
            {"targetPath", "D:/Course/Year4/BS/examples"},
            {"sourcesinkPath", "D:/Course/Year4/QLextension/Json/CWE-89-QL.json"}
        }}
    };

    client.sendNotification("workspace/didChangeConfiguration", {
        {"settings", config}
    });

    
    // 在初始化后添加Maven项目验证
    std::string mvnCmd = "mvn -f \"" + rootPath + "/pom.xml\" validate";
    std::system(mvnCmd.c_str());
    
    // 等待一段时间，确保服务器处理配置
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 打开文档并等待分析
    client.openDocument(fileUri, fileContent, "java");
    
    std::cout << "等待分析结果..." << std::endl;

    int waitTimeSeconds = 5; 
    for (int i = 0; i < waitTimeSeconds; i++) {
        // 检查是否已经收到诊断信息
        auto diagnostics = client.getDiagnostics(fileUri);
        if (!diagnostics.empty()) {
            std::cout << "已收到诊断信息，停止等待" << std::endl;
            break;
        }
        
        std::cout << "已等待 " << (i+1) << " 秒" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 记录分析完成时间
    auto analysisEnd = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(analysisEnd - analysisStart).count();
    std::cout << "分析耗时: " << duration << " ms" << std::endl;

    std::cout << "Finish  Analysis" << std::endl;
    
    // 获取诊断结果
    auto diagnostics = client.getDiagnostics(fileUri);
    std::cout << "收到 " << diagnostics.size() << " 条诊断信息" << std::endl;
    
    for (const auto& diag : diagnostics) {
        std::cout << "诊断: " << diag.dump(2) << std::endl;
    }
     
    client.executeCommand("echo \"Hello World!\"");

    // 关闭连接
    client.shutdown();
    
    // 添加强制退出代码
    std::cout << "程序即将退出..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 给一点时间让消息打印出来
    // exit(0); // 强制退出程序
    
    return 0;
}