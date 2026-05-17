# ==========================================
# 1. 静默检查所有依赖环境 (不使用 REQUIRED)
# ==========================================
find_package(OpenCV QUIET)
find_package(Threads QUIET)
find_package(nlohmann_json QUIET)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    # 静默查找 GStreamer 系列
    pkg_check_modules(GSTREAMER QUIET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
endif()

# 如果有任何一个核心依赖找不到，安全退出，不报错
if(NOT OpenCV_FOUND OR NOT GSTREAMER_FOUND)
    message(STATUS "[静默跳过] 缺少 OpenCV 或 GStreamer 环境，edge_board 文件夹不编译。")
    return() 
endif()

# ==========================================
# 2. 自动搜索当前目录所有源文件 (随时新增 .cpp，无需改这里)
# ==========================================
file(GLOB_RECURSE EDGE_SRCS CONFIGURE_DEPENDS
    "app/*.cpp"
    "src/*.cpp"
    "thread/*.cpp"
)

# ==========================================
# 3. 编译主可执行文件 (联合 shared)
# ==========================================
add_executable(edge_app ${EDGE_SRCS} ${SHARED_SRCS})

# 指定头文件包含路径
target_include_directories(edge_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_INC_DIR}
    ${OpenCV_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)

# 链接库 (nlohmann_json 单独判断一下，增强兼容性)
target_link_libraries(edge_app PRIVATE
    ${OpenCV_LIBS}
    ${GSTREAMER_LIBRARIES}
    Threads::Threads
)

if(nlohmann_json_FOUND)
    target_link_libraries(edge_app PRIVATE nlohmann_json::nlohmann_json)
endif()