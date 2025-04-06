#include "lsp_client.h"
#include "file_utils.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <cstring> // For memcpy

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
// Add network-related headers for Linux
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

LSPClient::LSPClient() : requestId(1), serverIn(nullptr), serverOut(nullptr) {}

LSPClient::~LSPClient() {
    shutdown();
}
bool LSPClient::initialize(const std::string& serverPath) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;

    // 创建管道
    if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ||
        !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0)) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }

    // 确保子进程不会继承写入端
    SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

    // 启动服务器进程
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdInput = hChildStdinRd;
    si.hStdOutput = hChildStdoutWr;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcess(
        NULL,
        const_cast<LPWSTR>(std::wstring(serverPath.begin(), serverPath.end()).c_str()),
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi)) {
        std::cerr << "Failed to start server process" << std::endl;
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdinRd);
    CloseHandle(hChildStdoutWr);

    // 打开文件句柄
    int inFd = _open_osfhandle((intptr_t)hChildStdoutRd, _O_RDONLY | _O_BINARY);
    int outFd = _open_osfhandle((intptr_t)hChildStdinWr, _O_WRONLY | _O_BINARY);

    serverIn = _fdopen(inFd, "rb");
    serverOut = _fdopen(outFd, "wb");
#else
    // Linux 实现
    int stdinPipe[2];  // 用于向子进程写入数据
    int stdoutPipe[2]; // 用于从子进程读取数据

    if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Failed to fork process" << std::endl;
        return false;
    }

    if (pid == 0) {  // 子进程
        // 设置标准输入输出
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);

        // 关闭不需要的管道端
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdinPipe[0]);
        close(stdoutPipe[1]);

        // 分解命令行参数
        execlp("java", "java", "-jar", serverPath.c_str(), "--stdio", NULL);
        perror("Failed to execute java");
        exit(1);
    }

    // 父进程
    // 关闭不需要的管道端
    close(stdinPipe[0]);
    close(stdoutPipe[1]);

    // 打开文件描述符
    serverOut = fdopen(stdinPipe[1], "w");
    serverIn = fdopen(stdoutPipe[0], "r");
#endif

    if (!serverIn || !serverOut) {
        std::cerr << "Failed to open server pipes" << std::endl;
        return false;
    }

    // 打开一个Java文件进行分析
    std::string filePath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src/main/java/tutorial1/Main.java";
    std::string srcPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src";
    std::string rootPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master";
    
    // 使用标准URI转换
    std::string fileUri = FileUtils::pathToUri(filePath);
    std::string rootUri = FileUtils::pathToUri(rootPath);

    // 发送初始化请求
    json initParams = {
        {"processId", ::getpid()},
        {"clientInfo", {
            {"name", "C++ Client"},
            {"version", "1.0"}
        }},
        {"rootUri", rootUri},
        {"workspaceFolders", json::array({
            {{"uri", rootUri}, {"name", "DemoProject"}}
        })},
        {"capabilities", {
            {"workspace", {
                {"configuration", true},
                {"workspaceFolders", true},
                // 修改这里：didChangeConfiguration应该是一个对象，而不是简单值
                {"didChangeConfiguration", {
                    {"dynamicRegistration", true}
                }}
            }},
            {"textDocument", {
                {"synchronization", {
                    {"didSave", true},
                    {"dynamicRegistration", true}
                }},
                {"diagnostics", {"relatedInformation", true}}
            }}
        }},
        // 其他参数保持不变
        // {"initializationOptions", {
        //     {"java", {
        //         {"sourcePaths", json::array({FileUtils::pathToUri(srcPath)})},
        //         {"project", {
        //             {"type", "maven"},
        //             {"buildFile", FileUtils::pathToUri(rootPath + "/pom.xml")}
        //         }},
        //         {"analysis", {
        //             {"mavenHome", "/home/user/.m2"},
        //             {"dependencyResolution", {
        //                 {"mode", "hybrid"}
        //             }}
        //         }}
        //     }}
        // }}
    };

    // 发送初始化请求并等待响应
    json response = sendRequest("initialize", initParams);
    
    // 检查响应是否有效
    if (response.empty() || (response.contains("error") && !response["error"].is_null())) {
        std::cerr << "初始化失败: " << response.dump(2) << std::endl;
        return false;
    }
    
    // 等待一小段时间再发送initialized通知
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 发送initialized通知
    sendNotification("initialized", json::object());
        
    // 启动消息监听器
    startMessageListener();

    return true;
}

json LSPClient::sendRequest(const std::string& method, const json& params) {
    json request = {
        {"jsonrpc", "2.0"},
        {"id", requestId++},
        {"method", method},
        {"params", params}
    };

    std::string content = request.dump();
    std::string message = "Content-Length: " + std::to_string(content.length()) + "\r\n\r\n" + content;
    
    fwrite(message.c_str(), 1, message.length(), serverOut);
    fflush(serverOut);
    
    // 读取服务器响应
    json response = readResponse();
    
    // 如果是不支持的方法，尝试从通知中获取信息
    if (method == "source" && response.empty()) {
        // 再次读取一次，可能有通知消息
        response = readResponse();
    }
    
    return response;
}

json LSPClient::readResponse() {
    char header[1024];
    int contentLength = -1;
    
    // 读取头部，查找 Content-Length
    while (fgets(header, sizeof(header), serverIn)) {
        if (strncmp(header, "Content-Length: ", 16) == 0) {
            contentLength = atoi(header + 16);
        } else if (strcmp(header, "\r\n") == 0) {
            // 头部结束
            break;
        }
    }
    
    if (contentLength <= 0) {
      // 如果客户端正在关闭，不输出错误信息
      if (isRunning) {
        std::cerr << "无效的 Content-Length 或未找到" << std::endl;
    }
        return json::object();
    }
    
    // 读取消息内容
    std::vector<char> buffer(contentLength + 1);
    size_t bytesRead = fread(buffer.data(), 1, contentLength, serverIn);
    buffer[bytesRead] = '\0';
    
    try {
        return json::parse(buffer.data());
    } catch (const std::exception& e) {
        // 如果客户端正在关闭，不输出错误信息
        if (isRunning) {
            std::cerr << "解析响应失败: " << e.what() << std::endl;
        }
        return json::object();
    }
}

void LSPClient::sendNotification(const std::string& method, const json& params) {
    json notification = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };

    std::string content = notification.dump();
    std::string message = "Content-Length: " + std::to_string(content.length()) + "\r\n\r\n" + content;
    
    fwrite(message.c_str(), 1, message.length(), serverOut);
    fflush(serverOut);
}

void LSPClient::openDocument(const std::string& uri, const std::string& text, const std::string& languageId) {
    // 确保 URI 格式正确
    std::string normalizedUri = uri;
    if (normalizedUri.find("file:///") != 0) {
        normalizedUri = "file:///" + normalizedUri;
    }
    
    json params = {
        {"textDocument", {
            {"uri", normalizedUri},
            {"languageId", languageId},
            {"version", 1},
            {"text", text}
        }}
    };
    
    sendNotification("textDocument/didOpen", params);
}

std::vector<json> LSPClient::getDiagnostics(const std::string& uri) {
    // 实际应用中，诊断信息通常是通过服务器推送的
    // 这里简化处理，返回存储的诊断信息
    if (diagnostics.find(uri) != diagnostics.end()) {
        return diagnostics[uri];
    }
    return {};
}

void LSPClient::shutdown() {
    if (serverIn && serverOut) {
        // 先停止监听线程
        isRunning = false;
        
        // 给线程一点时间退出
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // // 发送shutdown请求 这是告知服务器Client已经退出
        // sendRequest("shutdown", json::object());

        // 发送shutdown请求 这是告知服务器Client已经退出
        json response = sendRequest("shutdown", json::object());
        std::cout << "服务器响应shutdown请求: " << response.dump(2) << std::endl;
        
        // 发送exit通知 这是告知Server退出的指令
        // sendNotification("exit", json::object());
        
        // 再次确认监听线程已退出
        std::cout << "等待监听线程退出..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 获取文件描述符以便正确关闭
        int inFd = fileno(serverIn);
        int outFd = fileno(serverOut);
        
        // 关闭文件流
        fclose(serverIn);
        fclose(serverOut);
        serverIn = nullptr;
        serverOut = nullptr;
        close(inFd);
        close(outFd);
        std::cout << "LSP客户端已关闭" << std::endl;
    }
}

void LSPClient::documentDidChange(const std::string& uri, const std::string& newContent, int version) {
    json params = {
        {"textDocument", {
            {"uri", uri},
            {"version", version}
        }},
        {"contentChanges", {{
            {"text", newContent}
        }}}
    };
    sendNotification("textDocument/didChange", params);
}

json LSPClient::requestCompletion(const std::string& uri, int line, int character) {
    json params = {
        {"textDocument", {{"uri", uri}}},
        {"position", {
            {"line", line},
            {"character", character}
        }}
    };
    return sendRequest("textDocument/completion", params);
}

json LSPClient::requestDefinition(const std::string& uri, int line, int character) {
    json params = {
        {"textDocument", {{"uri", uri}}},
        {"position", {
            {"line", line},
            {"character", character}
        }}
    };
    return sendRequest("textDocument/definition", params);
}

void LSPClient::documentDidSave(const std::string& uri) {
    json params = {
        {"textDocument", {
            {"uri", uri}
        }}
    };
    sendNotification("textDocument/didSave", params);
}

json LSPClient::readMessage(int timeoutMs) {
    // 检查是否有数据可读
    fd_set readSet;
    FD_ZERO(&readSet);
    
    int fd = fileno(serverIn);
    FD_SET(fd, &readSet);
    
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    
    int result = select(fd + 1, &readSet, NULL, NULL, &timeout);
    
    if (result > 0) {
        // 有数据可读
        return readResponse();
    }
    
    // 超时或错误
    return json::object();
}

void LSPClient::updateDiagnostics(const std::string& uri, const json& diagnosticsData) {
    diagnostics[uri] = diagnosticsData;
}


json LSPClient::requestSourceName() {
    json params = {
        {"command", "getAnalyzerSource"},
        {"arguments", json::array()}
    };
    return sendRequest("workspace/executeCommand", params);
}

json LSPClient::executeCommand(const std::string& command) {
    json params = {
        {"command", command}
    };
    return sendRequest("workspace/executeCommand", params);
}

// 处理服务器消息
void LSPClient::processServerMessages(int timeoutMs) {
    json message = readMessage(timeoutMs);
    
    if (!message.empty()) {
        // 输出接收到的消息（调试用）
        std::cout << "服务器原始消息: " << message.dump(2) << std::endl;
        
        // 处理不同类型的消息
        if (message.contains("method")) {
            std::string method = message["method"];
            
            // 处理诊断信息
            if (method == "textDocument/publishDiagnostics" && message.contains("params")) {
                json params = message["params"];
                if (params.contains("uri") && params.contains("diagnostics")) {
                    std::string uri = params["uri"];
                    json diagArray = params["diagnostics"];
                    
                    // 更新诊断信息
                    diagnostics[uri] = diagArray;
                    
                    std::cout << "收到诊断信息: " << uri << " 包含 " 
                              << diagArray.size() << " 个问题" << std::endl;
                }
            }
            // 处理服务器消息
            else if (method == "window/showMessage" && message.contains("params")) {
                json params = message["params"];
                if (params.contains("message")) {
                    std::string msg = params["message"];
                    int type = params.value("type", 1);
                    
                    std::cout << "服务器消息 [" << type << "]: " << msg << std::endl;
                }
            }
            // 可以添加更多消息类型的处理...
        }
        // 处理响应消息
        else if (message.contains("id") && message.contains("result")) {
            // 这里处理请求的响应
            std::cout << "收到请求响应，ID: " << message["id"] << std::endl;
        }
    }
}

// 启动消息监听线程
void LSPClient::startMessageListener() {
    isRunning = true; // 设置运行标志
    // 创建一个后台线程来监听消息
    std::thread([this]() {
        std::cout << "开始监听服务器日志..." << std::endl;
        while (isRunning && serverIn != nullptr) {  // 检查运行标志
            this->processServerMessages(100);  // 100ms超时
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::cout << "服务器日志监听线程已退出" << std::endl;
    }).detach();  
}

bool LSPClient::connectToServer(const std::string& host, int port) {
    // 创建socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "创建socket失败" << std::endl;
        return false;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    // 解析主机名或IP地址
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        std::cerr << "无法解析服务器地址: " << host << std::endl;
        close(sock);
        return false;
    }
    
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    // 连接到服务器
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "连接服务器失败: " << host << ":" << port << std::endl;
        close(sock);
        return false;
    }

    // 将socket转换为FILE*以便使用现有代码
    serverIn = fdopen(sock, "r");
    serverOut = fdopen(dup(sock), "w");
    
    if (!serverIn || !serverOut) {
        std::cerr << "打开服务器连接失败" << std::endl;
        if (serverIn) fclose(serverIn);
        if (serverOut) fclose(serverOut);
        close(sock);
        return false;
    }

    // 打开一个Java文件进行分析
    std::string filePath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src/main/java/tutorial1/Main.java";
    std::string srcPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src";
    std::string rootPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master";
    
    // 使用标准URI转换
    std::string fileUri = FileUtils::pathToUri(filePath);
    std::string rootUri = FileUtils::pathToUri(rootPath);

    // 发送初始化请求 - 与initialize方法保持一致
    json initParams = {
        {"processId", ::getpid()},
        {"clientInfo", {
            {"name", "C++ Client"},
            {"version", "1.0"}
        }},
        {"rootUri", rootUri},
        {"workspaceFolders", json::array({
            {{"uri", rootUri}, {"name", "DemoProject"}}
        })},
        {"capabilities", {
            {"workspace", {
                {"configuration", true},
                {"workspaceFolders", true},
                {"didChangeConfiguration", {
                    {"dynamicRegistration", true}
                }}
            }},
            {"textDocument", {
                {"synchronization", {
                    {"didSave", true},
                    {"dynamicRegistration", true}
                }},
                {"diagnostics", {"relatedInformation", true}}
            }}
        }}
    };

    // 发送初始化请求并等待响应
    json response = sendRequest("initialize", initParams);
    
    // 检查响应是否有效
    if (response.empty() || (response.contains("error") && !response["error"].is_null())) {
        std::cerr << "初始化失败: " << response.dump(2) << std::endl;
        return false;
    }
    
    // 等待一小段时间再发送initialized通知
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 发送initialized通知
    sendNotification("initialized", json::object());
    
    // 启动消息监听器
    startMessageListener();
    
    return true;
}

// 添加心跳机制
void LSPClient::startHeartbeat() {
    std::thread([this]() {
        while (isRunning && serverIn != nullptr && serverOut != nullptr) {
            // 每30秒发送一次ping请求
            json pingParams = {};
            sendNotification("$/ping", pingParams);
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }).detach();
}

