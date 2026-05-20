# 优化建议与动态调参方案

本文档整理当前 `Qt_Vision` 工程中值得优化的部分，以及 PC 端 Qt 界面动态创建 Slider 并控制 edge 端变量的实现思路。

## 1. 当前架构简述

项目现在通过一个顶层 `CMakeLists.txt` 区分编译目标：

- x86/x64 Ubuntu PC 编译 `host_station`，作为 Qt 上位机。
- ARM/aarch64 Ubuntu 板卡编译 `edge_board`，作为边缘端视觉处理程序。

当前主要链路：

- edge 端采集 USB Camera 图像。
- edge 端通过 RDK X5 硬件编码 H.264。
- edge 端通过 UDP/GStreamer 推视频流到 PC。
- host 端通过 Qt + GStreamer 接收并显示视频。
- edge 端开启 TCP server，每秒向 host 端发送一行 JSON 状态数据。
- host 端通过 `SystemMonitor` 解析状态数据并显示 CPU、BPU、温度、FPS、分辨率等信息。

## 2. 当前值得优化的部分

### 2.1 IP 和端口硬编码

当前代码里存在固定 IP 和端口：

- host 端 TCP 连接 edge：`192.168.127.10:8888`
- edge 端 UDP 推流到 PC：`192.168.127.1:8888`
- 视频流和 TCP 状态都使用 `8888`

建议：

- 把 IP、端口、相机分辨率、FPS 移到配置文件，例如 `config.json`。
- 或者使用 CMake option / 命令行参数传入。
- 将视频端口和控制端口分开，例如：
  - `8888/UDP`：视频流。
  - `8890/TCP`：状态监控和参数控制。

这样可以避免同一端口承担多个语义，也方便以后部署到不同网络环境。

### 2.2 host 端重连逻辑不完整

当前 `host_station/src/systemmonitor.cpp` 中：

- 构造函数会主动连接 edge。
- 如果连接成功后断开，会在 `disconnected` 信号里 3 秒后重连。
- 但如果 host 先启动，而 edge 还没有启动，首次连接通常会触发 `ConnectionRefusedError` 或超时错误；当前 `onSocketError()` 只打印日志，没有安排下一次连接。

建议：

- 在 socket error 后也安排延迟重连。
- 避免在 `ConnectingState`、`HostLookupState`、`ConnectedState` 中重复调用 `connectToHost()`。
- 增加一个 `QTimer` 作为统一重连定时器。

推荐逻辑：

```cpp
void SystemMonitor::scheduleReconnect()
{
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(3000);
    }
}

void SystemMonitor::onSocketError()
{
    emit newSysLog("ERROR", "SOCKET", m_socket->errorString());
    m_socket->abort();
    scheduleReconnect();
}
```

### 2.3 控制通道和状态通道建议双向化

当前 edge 的 `NetworkMonitorThread()` 只负责：

- `accept()` host 连接。
- 每秒 `send()` JSON 状态。
- 如果发送失败，则等待下一次连接。

它没有读取 host 发来的控制命令。因此要实现动态 Slider，需要补上从 TCP socket 读取命令的逻辑。

建议有两种方式：

1. 保留一个 TCP 连接，同时双向传输。
   - edge 每秒发送状态 JSON。
   - host 随时发送参数控制 JSON。
   - edge 端使用 `select()` / `poll()` / 非阻塞 socket 同时处理读写。

2. 拆分两个 TCP 端口。
   - `status_port`：edge -> host 状态上报。
   - `control_port`：host -> edge 参数控制。
   - 逻辑更清楚，代码也更容易维护。

对于当前工程，我更推荐第二种：视频、状态、控制三条链路语义清晰，调试时也更容易定位问题。

### 2.4 源码中文注释编码

目前在 Windows 终端中读取代码时，中文注释显示为乱码。这通常是源文件编码、终端编码或 Git 编码设置不统一造成的。

建议：

- 所有源码统一保存为 UTF-8。
- `.editorconfig` 中明确：

```ini
[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
```

### 2.5 edge 端退出不够优雅

`server.cpp` 中已经通过 `g_running` 响应 `SIGINT` / `SIGTERM`，但 `Vision_thread()` 内部目前是 `while(1)`，不会主动读取退出标志。

建议：

- 将 `g_running` 传入视觉线程，或者在头文件中声明为 `extern std::atomic<bool> g_running`。
- 将 `while(1)` 改为 `while(g_running.load())`。
- 退出时关闭相机、编码器、GStreamer pipeline 和 socket。

## 3. 动态 Slider 调参需求

需求描述：

在 PC 端 Qt 上位机里点击创建一个类似 OpenCV trackbar 的 Slider。创建时填写：

- Slider 名称或显示名。
- 要绑定的变量名。
- 最小值。
- 最大值。
- 初始值。

之后用户拖动 Slider，host 端将变量名和值发送给 edge 端。edge 端根据变量名找到对应参数，并把新值用于视觉处理逻辑。

结论：可以实现，而且比较适合你当前的架构。

## 4. 不建议“直接通过变量名找 C++ 变量”

C++ 本身没有像 Python 那样方便的运行时变量反射。也就是说，edge 端不能可靠地通过字符串 `"h_min"` 直接找到一个同名 C++ 局部变量。

不推荐做法：

```cpp
// 不现实，也不安全
setVariableByName("h_min", 45);
```

推荐做法是使用“参数注册表”。

## 5. 推荐方案：参数注册表

edge 端将允许被调节的变量主动注册到一个表里：

```cpp
std::atomic<int> g_h_min{30};
std::atomic<int> g_h_max{150};
std::atomic<int> g_s_min{43};
std::atomic<int> g_s_max{255};

std::unordered_map<std::string, std::atomic<int>*> g_paramTable = {
    {"h_min", &g_h_min},
    {"h_max", &g_h_max},
    {"s_min", &g_s_min},
    {"s_max", &g_s_max},
};
```

收到 host 命令后：

```cpp
auto it = g_paramTable.find(name);
if (it != g_paramTable.end()) {
    it->second->store(value);
}
```

视觉线程使用时：

```cpp
int hMin = g_h_min.load();
int hMax = g_h_max.load();
```

优点：

- 安全：只有注册过的变量允许被修改。
- 清楚：变量名和真实参数一一对应。
- 线程安全：使用 `std::atomic<int>` 可以避免 UI 控制线程和视觉线程同时读写造成数据竞争。
- 可扩展：后续可以支持 `float`、`bool`、枚举、字符串等类型。

## 6. 推荐 JSON 协议

建议仍然使用一行一个 JSON，也就是 JSON line。

### 6.1 host 创建参数控件

```json
{"type":"param_create","name":"h_min","label":"H Min","min":0,"max":180,"value":30}
```

说明：

- `type`：消息类型。
- `name`：edge 端参数注册表里的参数名。
- `label`：Qt 界面显示名称。
- `min`：Slider 最小值。
- `max`：Slider 最大值。
- `value`：初始值。

### 6.2 host 修改参数值

```json
{"type":"param_set","name":"h_min","value":45}
```

### 6.3 edge 回复确认

```json
{"type":"param_ack","name":"h_min","value":45,"ok":true}
```

如果参数不存在：

```json
{"type":"param_ack","name":"unknown_param","ok":false,"error":"parameter not found"}
```

## 7. Qt 端实现思路

建议新增一个 C++ 类，例如 `ParamClient`：

```cpp
class ParamClient : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void createParam(const QString& name, int min, int max, int value);
    Q_INVOKABLE void setParam(const QString& name, int value);
};
```

它负责：

- 维护 TCP 控制连接。
- 将 QML 操作转换成 JSON。
- 发送到 edge。
- 接收 edge 的 ack。
- 将连接状态、错误信息、参数 ack 反馈给 QML。

QML 侧用 `ListModel` 存动态参数：

```qml
ListModel {
    id: paramModel
}

Repeater {
    model: paramModel
    delegate: Column {
        Text {
            text: model.label + ": " + slider.value.toFixed(0)
        }

        Slider {
            id: slider
            from: model.minValue
            to: model.maxValue
            value: model.value
            onMoved: paramClient.setParam(model.name, Math.round(value))
        }
    }
}
```

用户点击“创建 Slider”时：

1. QML 弹出输入框。
2. 用户输入参数名、最小值、最大值、初始值。
3. QML 将这个参数 append 到 `paramModel`。
4. QML 调用 `paramClient.createParam(...)` 或直接调用 `paramClient.setParam(...)`。
5. edge 收到后检查参数是否存在。
6. edge 回复 ack。
7. QML 根据 ack 标记该 Slider 是否绑定成功。

## 8. Edge 端实现思路

edge 端建议新增 `ParamRegistry`：

```cpp
class ParamRegistry {
public:
    bool setInt(const std::string& name, int value);
    int getInt(const std::string& name, int defaultValue = 0) const;
    void registerInt(const std::string& name, std::atomic<int>* ptr, int min, int max);
};
```

它负责：

- 保存参数名和真实变量地址的映射。
- 检查参数是否存在。
- 检查数值是否超出范围。
- 写入真实变量。

控制线程收到 JSON 后做分发：

```cpp
if (j["type"] == "param_set") {
    std::string name = j["name"];
    int value = j["value"];
    bool ok = registry.setInt(name, value);
    sendAck(name, value, ok);
}
```

## 9. 启动顺序问题

你的问题是：现在是否必须 edge 先开启才能开启 host？能不能做到不管谁先开启都可以？

结论：

- 视频链路上，host 可以先开启。host 端 GStreamer `udpsrc` 监听 UDP 端口，edge 后启动后开始推流，host 理论上就能收到。
- 状态 TCP 链路上，现在更接近“edge 先开更稳”。因为 edge 是 TCP server，host 是 TCP client。host 先开时会尝试连接 edge，如果 edge 没启动，连接会失败。
- 但这不是架构限制，完全可以改成“不管谁先启动都可以”。

## 10. 推荐的启动顺序解耦方案

### 10.1 保持 edge 为 TCP server，host 自动重连

这是改动最小的方案。

edge：

- 启动后监听控制/状态端口。
- host 断开后继续 `accept()` 下一次连接。

host：

- 启动后立即显示 UI。
- 尝试连接 edge。
- 如果连接失败，不退出程序，只显示 `OFFLINE`。
- 每 2 到 3 秒自动重试。
- 连接成功后切换到 `ONLINE`。

这样：

- edge 先开：host 启动后直接连接成功。
- host 先开：host 显示离线并持续重连，edge 启动后自动连上。

这是当前工程最应该做的第一步。

### 10.2 视频接收也做成常驻监听

host 的 GStreamer 视频接收可以在 UI 启动时就打开，或者点击按钮后打开。

即使 edge 暂时没有推流，`udpsrc` 也可以先监听端口。edge 启动后开始发 RTP/H.264 包，host 再显示画面。

建议 UI 状态：

- `VIDEO WAITING`：视频管道已启动，但尚未收到帧。
- `VIDEO ONLINE`：已收到视频帧。
- `VIDEO ERROR`：GStreamer pipeline 出错。

### 10.3 控制命令需要排队或禁用

如果 host 先启动，此时 edge 未连接，用户拖动 Slider 会遇到“命令发不出去”的情况。

建议两种策略：

1. 离线时禁用 Slider。
   - 最简单。
   - UI 显示 `CONTROL OFFLINE`。
   - 连接成功后再允许发送。

2. 离线时缓存最后一次参数值。
   - 用户可以先调参数。
   - host 保存每个参数的最后值。
   - edge 连接成功后，host 一次性同步所有参数。

更推荐第二种，因为体验更接近真正的上位机：先开谁都无所谓，连接恢复后自动同步状态。

## 11. 建议的最终通信结构

推荐把三类数据分清楚：

```text
Edge Board                                Host Station
----------                                ------------
Camera -> Encode -> UDP video  ------->  GStreamer/Qt VideoOutput

Status TCP server  -------------------->  SystemMonitor
Control TCP server <--------------------  ParamClient
```

也可以状态和控制共用一个 TCP 连接：

```text
Edge Board                                Host Station
----------                                ------------
TCP JSON line  <----------------------->  Status + Param Control
UDP Video     ------------------------->  GStreamer Video
```

如果短期快速实现，共用一个 TCP 连接就够了。如果要长期维护，建议拆分状态端口和控制端口。

## 12. 推荐实施顺序

1. 先给 host 的 `SystemMonitor` 加完整自动重连，让 host 可以先启动。
2. 将端口区分为视频端口和控制端口。
3. 新增 `ParamRegistry`，在 edge 注册允许调节的参数。
4. 新增 `ParamClient`，在 host 端负责发送参数 JSON。
5. QML 使用 `ListModel + Repeater + Slider` 动态创建调参控件。
6. 加 ack 和错误提示，避免参数名写错后无反馈。
7. 后续再做参数配置保存，下次启动自动恢复。

## 13. 简短结论

动态 Slider 可以实现，核心不是“通过字符串直接找变量”，而是“host 发送参数名，edge 用参数注册表查找并写入对应变量”。

启动顺序也可以做到完全不敏感。当前代码已经具备一部分基础，但 host 端 TCP 首次连接失败后的持续重连需要补齐。补齐后就可以实现：

- edge 先启动，host 后启动：正常连接。
- host 先启动，edge 后启动：host 先显示离线，edge 启动后自动上线。
- 任意一端重启：另一端不退出，等待自动重连。

