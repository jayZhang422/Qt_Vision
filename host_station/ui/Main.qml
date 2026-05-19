import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import QtMultimedia // 用于 VideoOutput 和 QVideoSink
import host_station // 导入你自己在 CMake 里定义的 C++ 模块 URI

ApplicationWindow {
    id: root
    visible: true
    width: 1200
    height: 850
    title: "RDK X5 智能视觉终端 v3.0"
    color: "#05070A" // 深邃纯黑底色

    // --- 极客风格 UI 常量定义 ---
    readonly property color themeCyan: "#00FF9D"      // 核心高亮荧光绿
    readonly property color themeBlue: "#00E5FF"      // 辅助科技蓝
    readonly property color themeDark: "#0B0E14"      // 面板基调暗
    readonly property color themeBorder: "#161D28"    // 极细暗边框
    readonly property color textMain: "#E2E8F0"       // 主文字色
    readonly property color textGray: "#4A5568"       // 暗淡提示色
    readonly property string monoFont: "Consolas"     // 极客专属等宽字体

    // --- 全局控制状态变量 ---
    property bool showCoordinates: false              // 是否开启十字坐标瞄准
    property int stopwatchTime: 0                     // 计时器毫秒数

    // 背景流光微弱底影
    background: Rectangle {
        color: "#05070A"
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#0500FF9D" }
                GradientStop { position: 1.0; color: "#FF000000" }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // =================================================================
        // 左侧：工作台核心（上方视频区域 + 下方极客控制台）
        // =================================================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            // --- 1. 视频显示与 OSD 悬浮区 (16:9 黄金比例锁定) ---
            Rectangle {
                id: videoContainer
                Layout.fillWidth: true
                Layout.preferredHeight: width * 9 / 16 // 保持16:9比例
                color: "#000000"
                radius: 6
                border.color: themeBorder
                border.width: 1
                clip: true

                // 【核心升级】：纯 C++ 实现的极低延迟 GStreamer 接收器
                GstVideoReceiver {
                    id: customReceiver
                    // 将接收器与 QML 画布的安全内存池绑定
                    videoSink: videoOutput.videoSink 
                    
                    // 接收底层报错，直接打印在赛博终端里
                    onPipelineError: function(msg) {
                        appendLog("ERROR", "GSTR", msg)
                    }
                }

                // 视频渲染画布
                VideoOutput {
                    id: videoOutput
                    anchors.fill: parent
                    // 保持宽高比并填充满，不拉伸变形
                    fillMode: VideoOutput.PreserveAspectFit 
                }

                // --- 模拟雷达扫描动画 (未连接视频流时显示) ---
                Item {
                    anchors.centerIn: parent
                    width: 260; height: 260
                    // 【注意】这里我们绑定按钮的状态，未播放时显示动画
                    visible: !pipelineBtn.isPlaying 

                    Rectangle {
                        anchors.fill: parent; radius: width / 2
                        color: "transparent"; border.color: themeCyan; border.width: 1; opacity: 0.1
                    }
                    Rectangle { width: parent.width; height: 1; color: themeCyan; anchors.centerIn: parent; opacity: 0.15 }
                    Rectangle { width: 1; height: parent.height; color: themeCyan; anchors.centerIn: parent; opacity: 0.15 }
                    ConicalGradient {
                        anchors.fill: parent; angle: 0
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#4000FF9D" }
                            GradientStop { position: 0.2; color: "transparent" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                        NumberAnimation on rotation { from: 0; to: 360; duration: 3000; loops: Animation.Infinite }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 100
                    text: "► SYSTEM IDLE // AWAITING GSTREAMER PIPELINE LINK"
                    color: themeCyan
                    font.family: monoFont
                    font.pixelSize: 12
                    font.letterSpacing: 1
                    opacity: 0.5
                    visible: !pipelineBtn.isPlaying
                }

                // --- 动态十字交叉瞄准线 ---
                Item {
                    anchors.fill: parent
                    visible: root.showCoordinates && videoMouse.containsMouse

                    Rectangle {
                        x: videoMouse.mouseX; y: 0; width: 1; height: parent.height
                        color: themeBlue; opacity: 0.4
                    }
                    Rectangle {
                        x: 0; y: videoMouse.mouseY; width: parent.width; height: 1
                        color: themeBlue; opacity: 0.4
                    }
                }

                MouseArea {
                    id: videoMouse
                    anchors.fill: parent
                    hoverEnabled: root.showCoordinates
                }

                // --- OSD 信息悬浮层（扁平微磨砂半透风格） ---
                RowLayout {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 12
                    spacing: 10

                    // 分辨率显示
                    Rectangle {
                        color: "#A00B0E14"; radius: 4; width: 100; height: 24
                        border.color: "#3000FF9D"; border.width: 1
                        Row {
                            anchors.centerIn: parent; spacing: 4
                            Text { text: "RES:"; color: textGray; font.family: monoFont; font.pixelSize: 10; font.bold: true }
                            Text { text: sysMonitor.resolution; color: "white"; font.family: monoFont; font.pixelSize: 10; font.bold: true }
                        }
                    }

                    // FPS 显示
                    Rectangle {
                        color: "#A00B0E14"; radius: 4; width: 85; height: 24
                        border.color: "#3000FF9D"; border.width: 1
                        Row {
                            anchors.centerIn: parent; spacing: 4
                            Text { text: "FPS:"; color: textGray; font.family: monoFont; font.pixelSize: 10; font.bold: true }
                            Text {
                                text: sysMonitor.fps.toFixed(2)
                                color: sysMonitor.fps < 30 ? "#FF3366" : themeCyan
                                font.family: monoFont; font.pixelSize: 10; font.bold: true
                            }
                        }
                    }
                }

                // 右上角：实时坐标显示
                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 12
                    visible: root.showCoordinates
                    color: "#A00B0E14"; radius: 4; width: 115; height: 24
                    border.color: "#3000E5FF"; border.width: 1
                    Row {
                        anchors.centerIn: parent; spacing: 6
                        Text { text: "X:"; color: textGray; font.family: monoFont; font.pixelSize: 10; font.bold: true }
                        Text {
                            text: videoMouse.containsMouse ? Math.round(videoMouse.mouseX).toString() : "-"
                            color: "white"; font.family: monoFont; font.pixelSize: 10; font.bold: true
                        }
                        Text { text: "Y:"; color: textGray; font.family: monoFont; font.pixelSize: 10; font.bold: true }
                        Text {
                            text: videoMouse.containsMouse ? Math.round(videoMouse.mouseY).toString() : "-"
                            color: themeBlue; font.family: monoFont; font.pixelSize: 10; font.bold: true
                        }
                    }
                }
            }

            // --- 2. 极客终端日志区 ---
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: themeDark
                radius: 6
                border.color: themeBorder
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    // 控制台顶部工具条
                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "💻 TERMINAL CONSOLE"
                            color: textMain
                            font.family: monoFont
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 1
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            color: "#121824"
                            width: 100; height: 24; radius: 3
                            border.color: stopwatchTimer.running ? "#6000FF9D" : themeBorder
                            Row {
                                anchors.centerIn: parent; spacing: 4
                                Text { text: "⏱️"; font.pixelSize: 10; opacity: stopwatchTimer.running ? 1.0 : 0.4 }
                                Text {
                                    text: formatTime(root.stopwatchTime)
                                    color: stopwatchTimer.running ? themeCyan : "white"
                                    font.family: monoFont; font.pixelSize: 12; font.bold: true
                                }
                            }
                        }

                        Row {
                            spacing: 2
                            Button {
                                text: stopwatchTimer.running ? "暂停" : "开始"
                                width: 40; height: 24
                                font.pixelSize: 10
                                onClicked: stopwatchTimer.running ? stopwatchTimer.stop() : stopwatchTimer.start()
                            }
                            Button {
                                text: "复位"
                                width: 40; height: 24
                                font.pixelSize: 10
                                onClicked: { stopwatchTimer.stop(); root.stopwatchTime = 0 }
                            }
                        }

                        Rectangle { width: 1; height: 16; color: themeBorder }

                        Button {
                            text: "清除日志"
                            height: 24
                            font.pixelSize: 10
                            onClicked: logModel.clear()
                        }

                        Button {
                            text: root.showCoordinates ? "隐藏坐标" : "显示坐标"
                            height: 24
                            font.pixelSize: 10
                            contentItem: Text {
                                text: parent.text
                                color: root.showCoordinates ? themeBlue : "white"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: root.showCoordinates ? "#1500E5FF" : "#1AFFFFFF"
                                border.color: root.showCoordinates ? themeBlue : themeBorder
                                radius: 3
                            }
                            onClicked: root.showCoordinates = !root.showCoordinates
                        }
                    }

                    // 日志输出核心视窗
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#06080C"
                        radius: 4
                        border.color: "#121722"
                        border.width: 1

                        ListView {
                            id: logListView
                            anchors.fill: parent
                            anchors.margins: 8
                            model: logModel
                            clip: true
                            delegate: Item {
                                width: logListView.width
                                height: 18
                                Row {
                                    spacing: 8
                                    Text { text: timeStamp; color: textGray; font.family: monoFont; font.pixelSize: 11 }
                                    Text { text: "[" + type + "]"; color: type === "ERROR" ? "#FF3366" : (type === "GSTR" ? themeBlue : themeCyan); font.family: monoFont; font.pixelSize: 11 }
                                    Text { text: message; color: textMain; font.family: monoFont; font.pixelSize: 11 }
                                }
                            }
                            onCountChanged: logListView.positionViewAtEnd()
                        }
                    }
                }
            }
        }

        // =================================================================
        // 右侧：精密数据控制台（Telemetry面板 + 算法管理 + 调参面板）
        // =================================================================
        Rectangle {
            Layout.preferredWidth: 350
            Layout.fillHeight: true
            color: themeDark
            radius: 6
            border.color: themeBorder
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 24

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "VISION CONTROL"
                        color: "white"
                        font.family: monoFont; font.bold: true; font.pixelSize: 16; font.letterSpacing: 2
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: sysMonitor.fps > 0 ? themeCyan : "#FF3366"
                        Glow { anchors.fill: parent; radius: 6; samples: 12; color: parent.color; source: parent }
                    }
                    Text {
                        text: sysMonitor.fps > 0 ? "ONLINE" : "OFFLINE"
                        color: sysMonitor.fps > 0 ? themeCyan : "#FF3366"
                        font.family: monoFont; font.pixelSize: 11; font.bold: true
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: themeBorder }

                // --- 硬件遥测区 ---
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "■ RDK X5 METRICS ENGINE"; color: textGray; font.family: monoFont; font.pixelSize: 10; font.bold: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15

                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Text { text: "CPU UTIL"; color: textGray; font.family: monoFont; font.pixelSize: 9 }
                            Row {
                                spacing: 2
                                Text { text: sysMonitor.cpuUsage; color: "white"; font.family: monoFont; font.pixelSize: 22; font.bold: true }
                                Text { text: "%"; color: textGray; font.family: monoFont; font.pixelSize: 10; anchors.bottom: parent.bottom; anchors.bottomMargin: 4 }
                            }
                            Rectangle { Layout.fillWidth: true; height: 2; color: "#161D28"
                                Rectangle { width: parent.width * Math.min(sysMonitor.cpuUsage, 100) / 100; height: 2; color: "white" }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Text { text: "BPU (AI)"; color: textGray; font.family: monoFont; font.pixelSize: 9 }
                            Row {
                                spacing: 2
                                Text { text: sysMonitor.bpuUsage; color: themeCyan; font.family: monoFont; font.pixelSize: 22; font.bold: true }
                                Text { text: "%"; color: textGray; font.family: monoFont; font.pixelSize: 10; anchors.bottom: parent.bottom; anchors.bottomMargin: 4 }
                            }
                            Rectangle { Layout.fillWidth: true; height: 2; color: "#161D28"
                                Rectangle { width: parent.width * Math.min(sysMonitor.bpuUsage, 100) / 100; height: 2; color: themeCyan }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Text { text: "CORE TEMP"; color: textGray; font.family: monoFont; font.pixelSize: 9 }
                            Row {
                                spacing: 2
                                Text { text: sysMonitor.cpuTemp.toFixed(1); color: sysMonitor.cpuTemp > 65.0 ? "#FF3366" : "#FFAA00"; font.family: monoFont; font.pixelSize: 22; font.bold: true }
                                Text { text: "°C"; color: textGray; font.family: monoFont; font.pixelSize: 10; anchors.bottom: parent.bottom; anchors.bottomMargin: 4 }
                            }
                            Rectangle { Layout.fillWidth: true; height: 2; color: "#161D28"
                                Rectangle { width: parent.width * Math.min(sysMonitor.cpuTemp, 100) / 100; height: 2; color: sysMonitor.cpuTemp > 65.0 ? "#FF3366" : "#FFAA00" }
                            }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: themeBorder }

                // --- 算法处理开关模块 ---
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "■ PIPELINE PROCESSING MODULES"; color: textGray; font.family: monoFont; font.pixelSize: 10; font.bold: true }

                    Switch {
                        id: yoloSwitch
                        text: "YOLOv8 Object Target Detector"
                        checked: true
                        font.family: monoFont; font.pixelSize: 11
                    }
                    Switch {
                        id: hsvSwitch
                        text: "HSV Real-time Color Filter"
                        checked: true
                        font.family: monoFont; font.pixelSize: 11
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: themeBorder }

                // --- HSV 轴高精细调试滑块 ---
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text { text: "■ HSV CHROMATIC CALIBRATION"; color: textGray; font.family: monoFont; font.pixelSize: 10; font.bold: true }

                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        RowLayout {
                            Text { text: "HUE CHANNEL"; color: "white"; font.family: monoFont; font.pixelSize: 10; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: Math.round(hSlider.first.value) + " - " + Math.round(hSlider.second.value); color: themeCyan; font.family: monoFont; font.pixelSize: 11; font.bold: true }
                        }
                        RangeSlider {
                            id: hSlider; from: 0; to: 180; first.value: 30; second.value: 150; Layout.fillWidth: true
                            background: Rectangle { height: 2; color: "#1F2937"; radius: 1
                                Rectangle { x: hSlider.first.visualPosition * parent.width; width: (hSlider.second.visualPosition - hSlider.first.visualPosition) * parent.width; height: 2; color: themeCyan }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        RowLayout {
                            Text { text: "SATURATION"; color: "white"; font.family: monoFont; font.pixelSize: 10; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: Math.round(sSlider.first.value) + " - " + Math.round(sSlider.second.value); color: themeCyan; font.family: monoFont; font.pixelSize: 11; font.bold: true }
                        }
                        RangeSlider {
                            id: sSlider; from: 0; to: 255; first.value: 43; second.value: 255; Layout.fillWidth: true
                            background: Rectangle { height: 2; color: "#1F2937"; radius: 1
                                Rectangle { x: sSlider.first.visualPosition * parent.width; width: (sSlider.second.visualPosition - sSlider.first.visualPosition) * parent.width; height: 2; color: themeCyan }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        RowLayout {
                            Text { text: "VALUE INTENSITY"; color: "white"; font.family: monoFont; font.pixelSize: 10; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: Math.round(vSlider.first.value) + " - " + Math.round(vSlider.second.value); color: themeCyan; font.family: monoFont; font.pixelSize: 11; font.bold: true }
                        }
                        RangeSlider {
                            id: vSlider; from: 0; to: 255; first.value: 46; second.value: 255; Layout.fillWidth: true
                            background: Rectangle { height: 2; color: "#1F2937"; radius: 1
                                Rectangle { x: vSlider.first.visualPosition * parent.width; width: (vSlider.second.visualPosition - vSlider.first.visualPosition) * parent.width; height: 2; color: themeCyan }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true } 

                // --- 5. 底部流触发控制按钮 ---
                Button {
                    id: pipelineBtn
                    // 用一个自定义属性代替原本的 playbackState
                    property bool isPlaying: false 
                    text: isPlaying ? "TERMINAL VIDEO PIPELINE" : "INITIALIZE VIDEO PIPELINE"
                    
                    Layout.fillWidth: true
                    Layout.preferredHeight: 45

                    background: Rectangle {
                        color: parent.down ? "#00B36E" : (parent.hovered ? "#1AFF9D" : "transparent")
                        border.color: themeCyan
                        border.width: 1
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.hovered ? "#05070A" : themeCyan
                        font.family: monoFont; font.pixelSize: 12; font.bold: true; font.letterSpacing: 1
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    
                    // 【控制核心】：通过点击调用 C++ 层的低延迟 GStreamer 管道
                    onClicked: {
                        if (isPlaying) {
                            customReceiver.stop()
                            isPlaying = false
                            appendLog("GSTR", "PIPELINE", "Zero-latency GStreamer pipeline stopped.");
                        } else {
                            customReceiver.start()
                            isPlaying = true
                            appendLog("GSTR", "PIPELINE", "Igniting Zero-latency GStreamer pipeline...");
                        }
                    }
                }
            }
        }
    }

    // =================================================================
    // 纯前端模拟与逻辑支撑组件
    // =================================================================

    ListModel {
        id: logModel
        Component.onCompleted: {
            appendLog("INFO", "SYS", "RDK X5 Local Station UI v3.0 Render Engine ready.");
            appendLog("INFO", "SYS", "Waiting network socket layer link initialization...");
        }
    }

    function appendLog(type, component, message) {
        var d = new Date();
        var timeStr = Qt.formatDateTime(d, "hh:mm:ss.zzz");
        logModel.append({ "timeStamp": timeStr, "type": type, "message": "[" + component + "] " + message });
    }

    function formatTime(ms) {
        var totalSecs = Math.floor(ms / 1000);
        var minutes = Math.floor(totalSecs / 60);
        var seconds = totalSecs % 60;
        var tenths = Math.floor((ms % 1000) / 100);
        var minStr = (minutes < 10 ? "0" : "") + minutes;
        var secStr = (seconds < 10 ? "0" : "") + seconds;
        return minStr + ":" + secStr + "." + tenths;
    }

    // 监听来自 C++ systemmonitor.cpp 发送的真实信号，输出到日志界面
    Connections {
        target: sysMonitor
        function onNewSysLog(type, component, message) {
            appendLog(type, component, message)
        }
    }

    Timer {
        id: fakeLogTimer
        interval: 3200
        running: false 
        repeat: true
    }

    Timer {
        id: stopwatchTimer
        interval: 100
        running: false
        repeat: true
        onTriggered: root.stopwatchTime += 100
    }
}