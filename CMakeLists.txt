cmake_minimum_required(VERSION 3.10)
project(RDK_X5_Server)

# ==========================================
# ★★★ 全局编译配置 ★★★
# ==========================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# ★★★ 1. 寻找所有第三方依赖 ★★★
# ==========================================
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
find_package(nlohmann_json REQUIRED)

# ==========================================
# ★★★ 2. 引入子目录 (两级 CMake 的核心桥梁) ★★★
# 这行代码会告诉 CMake：去 src/source 里面找另一个 CMakeLists.txt 并执行它
# ==========================================
add_subdirectory(src/source)
add_subdirectory(app)

# ==========================================
# ★★★ 3. 定义主可执行文件 ★★★
# ==========================================
add_executable(rdk_server server.cpp)

# ==========================================
# ★★★ 4. 指定主程序需要的头文件路径 ★★★
# 必须把 src/include 包含进来，这样 server.cpp 才能 #include "xxx.h"
# ==========================================
target_include_directories(rdk_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${GSTREAMER_INCLUDE_DIRS}
)

# ==========================================
# ★★★ 5. 链接目标库 ★★★
# ==========================================
target_link_libraries(rdk_server 
    PRIVATE 
    server_core     
    app_module      # <--- 这是我们在子目录中编译出来的静态库名字
    ${OpenCV_LIBS} 
    Threads::Threads
    ${GSTREAMER_LIBRARIES}
    nlohmann_json::nlohmann_json  
)