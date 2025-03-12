#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class LSPClient {
public:
    LSPClient();
    ~LSPClient();

    // 初始化连接
    bool initialize(const std::string& serverPath);
    
    // 发送请求
    json sendRequest(const std::string& method, const json& params);
    
    // 发送通知
    void sendNotification(const std::string& method, const json& params);
    
    // 打开文档
    void openDocument(const std::string& uri, const std::string& text, const std::string& languageId);
    
    // 获取诊断信息
    std::vector<json> getDiagnostics(const std::string& uri);
    
    // 关闭连接
    void shutdown();

private:
    // 处理服务器响应
    void handleResponse();
    
    // 读写管道
    FILE* serverIn;
    FILE* serverOut;
    
    // 请求ID
    int requestId;
    
    // 存储诊断信息
    std::unordered_map<std::string, std::vector<json>> diagnostics;
};