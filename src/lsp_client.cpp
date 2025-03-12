#include "lsp_client.h"
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

    // 发送初始化请求
    json initParams = {
        {"processId", 
#ifdef _WIN32
            GetCurrentProcessId()
#else
            getpid()
#endif
        },
        {"rootUri", nullptr},  // 将 rootUri 设置为 null，让服务器自行决定
        {"capabilities", {
            {"textDocument", {
                {"publishDiagnostics", {{"relatedInformation", true}}},
                {"synchronization", {{"didSave", true}}}
            }}
        }}
    };

    json response = sendRequest("initialize", initParams);
    
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
    
    // 这里应该实现等待响应的逻辑
    // 简化版本，实际应用中需要更复杂的处理
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return json::object(); // 返回空对象，实际应该返回服务器响应
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