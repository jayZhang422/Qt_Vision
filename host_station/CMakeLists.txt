# ==========================================
# 1. 静默寻找 Qt6，包含必需的模块 (缺环境即跳过)
# ==========================================
list(APPEND CMAKE_PREFIX_PATH "/home/jayzhang/Qt/6.11.0/gcc_64")

find_package(Qt6 QUIET COMPONENTS Core Gui Qml Quick Core5Compat)
if(NOT Qt6_FOUND)
    message(STATUS "[静默跳过] 找不到 Qt6 环境，host_station 文件夹不编译。")
    return()
endif()

# ==========================================
# 2. Qt 自动化机制 (用于支持信号槽 MOC 等)
# ==========================================
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 沿用你的 Qt 标准项目设置
qt_standard_project_setup(REQUIRES 6.10)

# ==========================================
# 3. 自动扫描 C++ 与 QML 源码 (修复绝对路径缺陷)
# ==========================================
# C++ 源码允许使用绝对路径，保持原样
file(GLOB_RECURSE HOST_SRCS CONFIGURE_DEPENDS 
    "src/*.cpp"
    "include/*.h"
    "include/*.hpp"
)

# ★ 核心修复：添加 RELATIVE 参数，强制让 CMake 搜集相对于当前目录的 QML 相对路径
file(GLOB_RECURSE QML_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "ui/*.qml")

# ==========================================
# 4. 生成可执行文件 (明确 main 函数入口在 app/main.cpp)
# ==========================================
qt_add_executable(host_app app/main.cpp ${SHARED_SRCS})

# 使用 Qt6 特性，将 QML 和 对应的 C++ 后端一并注册为模块
qt_add_qml_module(host_app
    URI host_station
    VERSION 1.0
    QML_FILES ${QML_FILES}   # 此时传入的已经是安全的相对路径了
    SOURCES ${HOST_SRCS}
)

# 包含自己的头文件
target_include_directories(host_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_INC_DIR}
)

# 链接 Qt 库
target_link_libraries(host_app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Core5Compat
)