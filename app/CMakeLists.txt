# ==========================================
# app/CMakeLists.txt
# ==========================================

# 1. 把 app 目录下的所有源文件打包成一个静态库 (STATIC)，名字叫 app_module
add_library(app_module STATIC 
    thread_monitor.cpp
    # 如果以后有新的业务代码，比如 control_logic.cpp，直接往下面加：
    # control_logic.cpp
    # vision_task.cpp
)

# 2. 【核心魔法】告诉编译器头文件在哪
# 使用 PUBLIC 关键字非常关键！这意味：
# - 编译 app_module 自己时，能在当前目录找到 .h 文件
# - 谁在外面链接了 app_module，谁也就自动获得了这个目录的搜索权限！
target_include_directories(app_module PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})