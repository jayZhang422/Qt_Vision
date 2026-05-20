# Qt Vision

Qt Vision 是一个面向 Ubuntu PC 上位机和 Ubuntu ARM 边缘板卡的实时视觉工程。项目使用同一个顶层 `CMakeLists.txt`，根据编译机器的处理器架构自动选择构建目标：

- x86/x64 Ubuntu PC：构建 `host_station`，提供 Qt Quick 上位机界面、视频显示、系统状态监控和调参入口。
- ARM/aarch64 Ubuntu 板卡：构建 `edge_board`，负责相机采集、图像处理、硬件编码、UDP 视频推流和 TCP 状态上报。

当前代码主要适配 RDK X5 类边缘板卡，PC 端通过 Qt 界面接收低延迟 H.264 视频流，并通过 TCP 接收板端的运行状态。

## 功能概览

- 单仓库双端构建：顶层 CMake 根据 `CMAKE_SYSTEM_PROCESSOR` 自动进入 `host_station` 或 `edge_board`。
- PC 上位机：
  - Qt Quick/QML 科技风控制界面。
  - GStreamer 接收 UDP H.264 视频流并渲染到 `VideoOutput`。
  - TCP 连接板端，接收 FPS、分辨率、CPU、BPU、温度等状态。
  - 内置 HSV RangeSlider 调参界面雏形。
- 边缘板卡：
  - USB Camera 图像采集。
  - OpenCV 图像处理入口。
  - RDK X5 硬件 H.264 编码。
  - GStreamer UDP 推流到 PC。
  - TCP 服务端周期性发送 JSON 状态数据。

## 目录结构

```text
.
├── CMakeLists.txt                 # 顶层构建入口，自动区分 PC / ARM
├── host_station/                  # PC 上位机
│   ├── app/main.cpp               # Qt 应用入口
│   ├── ui/Main.qml                # 主界面
│   ├── include/                   # Qt 后端类声明
│   └── src/                       # Qt 后端实现
└── edge_board/                    # ARM 边缘板卡
    ├── app/server.cpp             # 板端程序入口
    ├── include/                   # 相机、编码、推流等接口
    ├── src/                       # 相机、编码、推流、系统监控实现
    ├── thread/                    # 网络监控和视觉线程
    └── auxiliary/                 # 全局配置和图像格式转换工具
```

## 网络链路

默认网络配置写在当前代码中：

| 数据方向 | 协议 | 默认地址/端口 | 说明 |
| --- | --- | --- | --- |
| Edge -> PC | UDP/RTP/H.264 | `192.168.127.1:8888` | 板端视频推流，PC 端 Qt/GStreamer 接收 |
| PC -> Edge | TCP | `192.168.127.10:8888` | PC 连接板端监控服务 |
| Edge -> PC | TCP JSON line | 同一 TCP 连接 | 板端每秒发送一行 JSON 状态 |

TCP 状态数据示例：

```json
{
  "camera_fps": 45,
  "cam_width": 1280,
  "cam_height": 720,
  "cpu_usage_percent": 12.3,
  "cpu_temperature_c": 42.8,
  "bpu_usage_percent": 30,
  "timestamp_ms": 1710000000000
}
```

## 依赖环境

### PC 上位机

- Ubuntu x86/x64
- CMake 3.16+
- C++17 编译器
- Qt 6.10+，当前 `host_station/CMakeLists.txt` 中默认追加了：

```cmake
/home/jayzhang/Qt/6.11.0/gcc_64
```

- GStreamer：

```bash
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

### ARM 边缘板卡

- Ubuntu ARM/aarch64
- CMake 3.16+
- C++17 编译器
- OpenCV
- GStreamer
- nlohmann_json
- libyuv
- RDK X5 编码相关库，例如当前代码链接的 `spcdev`

依赖安装方式会随板卡系统镜像不同而变化，建议优先使用板卡官方 SDK 或系统仓库。

## 构建方式

### 在 PC 上构建上位机

```bash
cmake -S . -B build
cmake --build build -j
./build/host_station/host_app
```

如果 Qt 不在默认路径，可以在配置时指定：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
```

### 在 ARM 板卡上构建边缘端

```bash
cmake -S . -B build
cmake --build build -j
./build/edge_board/edge_app
```

顶层 CMake 会检测到 `aarch64` 或 `arm`，自动进入 `edge_board`。

## 运行流程

1. 确认 PC 和板卡处于同一网络，并能互相访问。
2. 根据实际网络修改：
   - `edge_board/thread/thread_monitor.cpp` 中的 UDP 目标 IP。
   - `host_station/src/systemmonitor.cpp` 中连接板卡的 TCP IP。
3. 在板卡运行 `edge_app`。
4. 在 PC 运行 `host_app`。
5. 点击 Qt 界面的 `INITIALIZE VIDEO PIPELINE` 启动视频接收。

也可以先用 GStreamer 命令在 PC 端验证视频流：

```bash
gst-launch-1.0 udpsrc port=8888 caps="application/x-rtp, media=video, clock-rate=90000, encoding-name=H264" ! rtph264depay ! h264parse ! avdec_h264 ! autovideosink sync=false
```

## 动态 Slider 调参方案

可以实现“在 PC 端 Qt 界面点击创建类似 OpenCV trackbar 的 Slider，并把变量名、最小值、最大值和值发送给 edge 端”的功能。推荐不要让 edge 端直接通过 C++ 变量名反射访问变量，而是建立一个参数注册表。

推荐协议仍然使用 JSON line。PC 端创建参数：

```json
{"type":"param_create","name":"h_min","min":0,"max":180,"value":30}
```

PC 端修改参数：

```json
{"type":"param_set","name":"h_min","value":45}
```

Edge 端回复：

```json
{"type":"param_ack","name":"h_min","value":45,"ok":true}
```

Edge 端维护类似下面的参数表：

```cpp
std::unordered_map<std::string, std::atomic<int>*> params = {
    {"h_min", &g_h_min},
    {"h_max", &g_h_max},
    {"s_min", &g_s_min},
    {"s_max", &g_s_max},
};
```

收到 `param_set` 后，edge 端按 `name` 查表并写入对应原子变量。视觉线程只读取这些原子变量参与图像处理。这样比“通过字符串直接找变量”更安全、可控，也方便限制哪些参数允许被上位机修改。

Qt/QML 端可以用 `ListModel` + `Repeater` 动态生成 Slider：

```qml
ListModel {
    id: paramModel
}

Repeater {
    model: paramModel
    delegate: Slider {
        from: model.minValue
        to: model.maxValue
        value: model.value
        onMoved: paramClient.setParam(model.name, value)
    }
}
```

建议新增一个 PC 端 C++ 类，例如 `ParamClient`，负责：

- 连接 edge 端 TCP 控制通道。
- 提供 `Q_INVOKABLE createParam(name, min, max, value)`。
- 提供 `Q_INVOKABLE setParam(name, value)`。
- 把 JSON 命令发送给 edge。
- 接收 edge 的 ack 并通知 QML 更新状态。

Edge 端建议新增一个控制通道线程，或者把现有 TCP 监控线程改成双向协议：既能发送状态，也能读取 PC 发来的控制命令。

## 后续优化建议

- 将 IP、端口、相机分辨率、FPS 从源码常量改为配置文件或 CMake option。
- 避免 PC 端和板端都硬编码 `8888`，建议区分 `video_port` 和 `control_port`，例如视频 `8888/UDP`，控制 `8890/TCP`。
- 当前部分中文注释在 Windows/终端中显示为乱码，建议统一源码编码为 UTF-8。
- `Vision_thread()` 中主循环没有检查全局退出标志，收到 SIGINT 后主线程会退出等待，但视觉线程本身无法优雅收尾，建议传入或共享 `g_running`。
- `NetworkMonitorThread()` 当前只发状态，没有读取客户端控制命令；动态 Slider 功能需要补上 TCP 读取和 JSON 命令分发。
- `host_station/CMakeLists.txt` 中 Qt 路径是个人绝对路径，建议改成用户通过 `CMAKE_PREFIX_PATH` 传入。