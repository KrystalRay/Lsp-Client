# LSP-Client 使用说明
# 注意 该项目仍在建设中
## 项目简介

LSP-Client 是一个基于 C++ 实现的 Language Server Protocol (LSP) 客户端，专为与 Java 语言服务器通信而设计。该客户端能够连接到 MagpieBridge 服务器，并通过 LSP 协议进行代码分析和诊断。

## 功能特点

- 支持与 Java LSP 服务器的通信
- 能够打开和分析 Java 源代码文件
- 提供代码诊断信息收集
- 支持 Maven 项目的依赖解析
- 实时监听服务器消息和诊断结果

## 系统要求

- C++11 或更高版本
- CMake 3.10 或更高版本
- Java 运行环境 (JRE 8+)
- Maven (用于项目依赖解析)
- nlohmann/json 库 (用于 JSON 处理)

## 编译与安装

```bash
# 克隆仓库
git clone https://github.com/KrystalRay/LSP-Client.git
cd LSP-Client

# 创建构建目录
mkdir build && cd build

# 配置并编译
cmake ..
make
```

## 使用方法

1. 确保 Java 服务器 JAR 文件已准备就绪
2. 运行客户端程序，指定 Java 文件路径和服务器路径

```bash
./client
```

## 配置说明

客户端使用 JSON 格式的配置参数，主要包括：

- `sourcePaths`: Java 源代码路径
- `project`: 项目类型和构建文件路径
- `analysis`: 分析级别和依赖解析配置

## 示例代码

```cpp
// 创建LSP客户端
LSPClient client;

// 初始化连接到MagpieBridge服务器
std::string serverPath = "path/to/server.jar";
if (!client.initialize(serverPath)) {
    std::cerr << "Failed to initialize LSP client" << std::endl;
    return 1;
}

// 打开文档并等待分析
client.openDocument(fileUri, fileContent, "java");

// 获取诊断结果
auto diagnostics = client.getDiagnostics(fileUri);
```

## 项目结构

- `src/`: 源代码目录
  - `lsp_client.cpp/h`: LSP 客户端核心实现
  - `file_utils.cpp/h`: 文件操作工具类
  - `main.cpp`: 主程序入口

## 常见问题

1. **连接失败**: 确保 Java 服务器路径正确，且 Java 环境已正确配置
2. **无法获取诊断信息**: 检查源代码路径和项目配置是否正确
3. **依赖解析失败**: 确保 Maven 已正确安装，且 pom.xml 文件路径正确

## 贡献指南

欢迎提交 Pull Request 或 Issue 来帮助改进项目。在提交代码前，请确保：

1. 代码风格符合项目规范
2. 添加了必要的测试
3. 更新了相关文档

## 许可证

本项目采用 MIT 许可证。详情请参阅 LICENSE 文件。