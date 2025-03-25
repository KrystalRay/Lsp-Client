#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class FileUtils {
public:
    // 读取文件内容
    static std::string readFile(const std::string& path);
    
    // 将文件路径转换为LSP URI格式
    static std::string pathToUri(const std::string& path);

    // 统一路径格式
    static std::string normalizePath(const std::string& path);
    
    // 设置项目源代码路径
    static json createSourcePathConfig(const std::string& srcPath);
};