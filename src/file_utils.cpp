#include "file_utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>

std::string FileUtils::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return content;
}

json FileUtils::createSourcePathConfig(const std::string& srcPath) {
    json params;
    json sourcePaths = json::array();
    sourcePaths.push_back(srcPath);
    params["sourcePath"] = sourcePaths;
    
    return params;
}

std::string FileUtils::normalizePath(const std::string& path) {
    std::string normalized = path;
    // 统一斜杠方向
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    // 去除末尾斜杠
    if (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

std::string FileUtils::pathToUri(const std::string& path) {
    std::string normalized = normalizePath(path);
    
    // 确保路径以 / 开头
    if (!normalized.empty() && normalized[0] != '/') {
        normalized = '/' + normalized;
    }
    
    // 检查是否已经是 URI 格式
    if (normalized.find("file://") == 0) {
        return normalized;
    }
    
    return "file://" + normalized;
}