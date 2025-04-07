#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class LSPClient {
public:
    LSPClient();
    ~LSPClient();

    // 连接到服务器
    bool connectToServer(const std::string& host, int port);

    // 初始化连接
    bool initialize(const std::string& serverPath);
    
    // 发送请求
    json sendRequest(const std::string& method, const json& params);
    
    // 读取响应
    json readResponse();

    // 执行命令
    json executeCommand(const std::string& command);
    
    // 发送通知
    void sendNotification(const std::string& method, const json& params);
    
    // 打开文档
    void openDocument(const std::string& uri, const std::string& text, const std::string& languageId);
    
    // 获取诊断信息
    std::vector<json> getDiagnostics(const std::string& uri);
    
     
    // 文档变更通知
    void documentDidChange(const std::string& uri, const std::string& newContent, int version);
    
    // 代码补全请求
    json requestCompletion(const std::string& uri, int line, int character);
    
    // 转到定义请求
    json requestDefinition(const std::string& uri, int line, int character);
    
    // 保存文档通知
    void documentDidSave(const std::string& uri);
    
    // 非阻塞方式读取服务器消息，超时返回空对象
    json readMessage(int timeoutMs = 0);
    
    // 更新诊断信息
    void updateDiagnostics(const std::string& uri, const json& diagnosticsData);
    
    // 发送请求源名称
    json requestSourceName();

    // 处理服务器消息
    void processServerMessages(int timeoutMs);
    
    // 启动消息监听线程
    void startMessageListener();

    // 关闭连接
    void shutdown();
    
    // 退出服务器守护进程
    void serverExit();

    void startHeartbeat();
private:
    // 处理服务器响应
    void handleResponse();
    
    // 读写管道
    FILE* serverIn;
    FILE* serverOut;
    
    // 请求ID
    int requestId;
    
    // 消息监听线程
    std::atomic<bool> isRunning; 
    
    // 存储诊断信息
    std::unordered_map<std::string, std::vector<json>> diagnostics;
};