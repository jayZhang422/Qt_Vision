cmake_minimum_required(VERSION 3.16)
project(cpp_server LANGUAGES CXX)

# ==========================================
# ★★★ 全局编译配置 (完美支持 clangd 补全) ★★★
# ==========================================
# 这一行极其重要，它会生成 compile_commands.json 供 clangd 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# ★★★ 1. 智能平台架构嗅探 (区分 RDK X5 与 PC) ★★★
# ==========================================
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set(IS_EDGE_BOARD ON)
    message(STATUS "架构检测为 ARM/aarch64，目标为边缘板卡 (Edge Board)。")
else()
    set(IS_EDGE_BOARD OFF)
    message(STATUS "架构检测为 x86/x64，目标为上位机 (Host Station)。")
endif()

# ==========================================
# ★★★ 2. 收集共享目录源码 (支持自动增减文件) ★★★
# ==========================================
file(GLOB_RECURSE SHARED_SRCS CONFIGURE_DEPENDS "shared/*.cpp")
set(SHARED_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared)

# ==========================================
# ★★★ 3. 严格的分流机制 (环境不符直接跳过) ★★★
# ==========================================
if(IS_EDGE_BOARD)
    add_subdirectory(edge_board)
else()
    add_subdirectory(host_station)
endif()