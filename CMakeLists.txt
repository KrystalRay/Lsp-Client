cmake_minimum_required(VERSION 3.10)
project(LSP-Client)

# 主客户端可执行文件
add_executable(client 
${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/src/file_utils.cpp
${CMAKE_CURRENT_SOURCE_DIR}/src/lsp_client.cpp
)

# 添加exitServer可执行文件
add_executable(exitserver
${CMAKE_CURRENT_SOURCE_DIR}/src/exitServer.cpp
${CMAKE_CURRENT_SOURCE_DIR}/src/file_utils.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/src/lsp_client.cpp
)

# 为两个可执行文件添加相同的包含目录
target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include  
)

target_include_directories(exitserver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include  
)
