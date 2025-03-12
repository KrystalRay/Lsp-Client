cmake_minimum_required(VERSION 3.10)
project(LSP-Client)

add_executable(client 
${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/src/lsp_client.cpp)

# 添加包含目录
target_include_directories(client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include  
)
