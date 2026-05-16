# 子目录不需要写 cmake_minimum_required 和 project()

# 1. 收集当前目录下所有的 .cpp 源文件
# 将所有的源码文件名字保存到变量 DIR_SRCS 中
aux_source_directory(. DIR_SRCS)

# 【注意】如果你的 source 文件夹现在还是空的，没有任何 .cpp 文件，
# 上面那句话会报错。你可以先在 source 目录下新建一个空的 dummy.cpp 占位，
# 或者手动指定文件名，比如：add_library(server_core STATIC monitor.cpp)

# 2. 将这些源文件编译为一个静态库 (STATIC)，命名为 server_core
add_library(server_core STATIC ${DIR_SRCS})

# 3. 指定这个静态库在编译时去哪里找头文件
# PUBLIC 属性不仅让这个库本身能看到 include，还会把这个路径传递给用到它的 rdk_server
target_include_directories(server_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 4. （可选）如果你的内部业务代码也直接调用了 JSON 或 OpenCV，
# 建议在这里也把需要的库链接给 server_core
target_link_libraries(server_core 
    PUBLIC 
    nlohmann_json::nlohmann_json
    ${OpenCV_LIBS}
)