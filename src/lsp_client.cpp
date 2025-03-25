#include "lsp_client.h"
#include "file_utils.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
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
    std::string srcPath = "/mnt/d/Course/Year4/QLextension/DemoProject-master/src/main/java";
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
        {"initializationOptions", {
            {"java", {
                {"sourcePaths", json::array({FileUtils::pathToUri(srcPath)})},
                {"project", {
                    {"type", "maven"},
                    {"buildFile", FileUtils::pathToUri(rootPath + "/pom.xml")}
                }},
                {"analysis", {
                    {"mavenHome", "/home/user/.m2"},
                    {"dependencyResolution", {
                        {"mode", "hybrid"}
                    }}
                }}
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
        std::cerr << "无效的 Content-Length 或未找到" << std::endl;
        return json::object();
    }
    
    // 读取消息内容
    std::vector<char> buffer(contentLength + 1);
    size_t bytesRead = fread(buffer.data(), 1, contentLength, serverIn);
    buffer[bytesRead] = '\0';
    
    try {
        return json::parse(buffer.data());
    } catch (const std::exception& e) {
        std::cerr << "解析响应失败: " << e.what() << std::endl;
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
        // 发送shutdown请求
        sendRequest("shutdown", json::object());
        
        // 发送exit通知
        sendNotification("exit", json::object());
        
        fclose(serverIn);
        fclose(serverOut);
        serverIn = nullptr;
        serverOut = nullptr;
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
    // 不直接请求source方法，而是使用一个自定义方法来获取通知
    json params = {
        {"command", "getSourceName"}
    };
    return sendRequest("workspace/executeCommand", params);
}

