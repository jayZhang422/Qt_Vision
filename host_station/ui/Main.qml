import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

ApplicationWindow {
    visible: true
    width: 1200
    height: 800
    title: "RDK X5 智能视觉终端 v2.0"
    color: "#0B0E14"

    // 自定义字体颜色常量
    readonly property color themeCyan: "#00FF9D"
    readonly property color themeDark: "#1C222E"
    readonly property color themeBorder: "#303A4F"
    readonly property color textGray: "#8B95A5"

    // 1. 设置高级暗黑背景
    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0B0E14" }
            GradientStop { position: 1.0; color: "#151A24" }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // ================= 左侧：视频显示与 OSD 悬浮区 =================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: themeDark
            radius: 12
            border.color: themeBorder
            border.width: 1

            // 视频渲染占位框
            Rectangle {
                id: videoContainer
                anchors.margins: 20
                anchors.fill: parent
                color: "#000000"
                radius: 8
                clip: true

                // --- 模拟雷达扫描动画 (未连接时显示) ---
                Item {
                    anchors.centerIn: parent
                    width: 300; height: 300
                    Rectangle {
                        anchors.fill: parent; radius: width / 2
                        color: "transparent"; border.color: themeCyan; border.width: 1; opacity: 0.2
                    }
                    Rectangle { width: parent.width; height: 1; color: themeCyan; anchors.centerIn: parent; opacity: 0.2 }
                    Rectangle { width: 1; height: parent.height; color: themeCyan; anchors.centerIn: parent; opacity: 0.2 }
                    ConicalGradient {
                        anchors.fill: parent; angle: 0
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#8000FF9D" }
                            GradientStop { position: 0.15; color: "transparent" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                        NumberAnimation on rotation { from: 0; to: 360; duration: 2500; loops: Animation.Infinite }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 180
                    text: "WAITING FOR GSTREAMER PIPELINE..."
                    color: themeCyan
                    font.pixelSize: 14
                    font.letterSpacing: 2
                    opacity: 0.6
                }

                // --- OSD 信息悬浮层 (叠加在视频之上) ---
                RowLayout {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 15
                    spacing: 15

                    // 1. 分辨率
                    Rectangle {
                        color: "#80000000"; radius: 4; width: 110; height: 26
                        border.color: "#4000FF9D"; border.width: 1
                        Row {
                            anchors.centerIn: parent
                            spacing: 5
                            Text { text: "RES:"; color: "white"; font.pixelSize: 11; font.bold: true }
                            Text {
                                text: sysMonitor.resolution
                                color: themeCyan; font.pixelSize: 11; font.bold: true
                            }
                        }
                    }

                    // 2. 帧率
                    Rectangle {
                        color: "#80000000"; radius: 4; width: 80; height: 26
                        border.color: "#4000FF9D"; border.width: 1
                        Row {
                            anchors.centerIn: parent
                            spacing: 5
                            Text { text: "FPS:"; color: "white"; font.pixelSize: 11; font.bold: true }
                            Text {
                                text: sysMonitor.fps.toFixed(2)
                                color: sysMonitor.fps < 30 ? "#FF3366" : themeCyan // 低帧率变红
                                font.pixelSize: 11; font.bold: true
                            }
                        }
                    }


                }
            }
        }

        // ================= 右侧：高级控制台 =================
        Rectangle {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            color: themeDark
            radius: 12
            border.color: themeBorder
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 25
                spacing: 25

                // 1. 标题栏
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "VISION CONTROL"
                        color: "white"; font.bold: true; font.pixelSize: 22; font.letterSpacing: 1
                        Layout.fillWidth: true
                    }
                    // 状态指示灯
                    Rectangle {
                        width: 10; height: 10; radius: 5; color: "#FF3366"
                        Glow { anchors.fill: parent; radius: 8; samples: 16; color: parent.color; source: parent }
                    }
                    Text { text: "OFFLINE"; color: textGray; font.pixelSize: 12; font.bold: true }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: themeBorder }

                // 2. 硬件遥测面板 (Telemetry)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        text: "RDK X5 TELEMETRY"
                        color: textGray
                        font.pixelSize: 12
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        // --- CPU 卡片 ---
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 65; color: "#252D3D"; radius: 6
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0
                                Text {
                                    text: "CPU"
                                    color: textGray
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom // 水平居中，靠下
                                }
                                RowLayout {
                                    spacing: 2
                                    Layout.alignment: Qt.AlignHCenter | Qt.AlignTop // 水平居中，靠上
                                    Text {
                                        text: sysMonitor.cpuUsage
                                        color: "white"; font.pixelSize: 20; font.bold: true
                                    }
                                    Text {
                                        text: "%"
                                        color: textGray; font.pixelSize: 12; font.bold: true
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 4 // 微调百分号的高度
                                    }
                                }
                            }
                        }

                        // --- BPU 卡片 ---
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 65; color: "#252D3D"; radius: 6
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0
                                Text {
                                    text: "BPU (AI)"
                                    color: textGray; font.pixelSize: 10; font.bold: true
                                    Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                                }
                                RowLayout {
                                    spacing: 2
                                    Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                                    Text {
                                        text: sysMonitor.bpuUsage
                                        color: themeCyan; font.pixelSize: 20; font.bold: true
                                    }
                                    Text {
                                        text: "%"
                                        color: textGray; font.pixelSize: 12; font.bold: true
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 4
                                    }
                                }
                            }
                        }

                        // --- 温度卡片 ---
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 65; color: "#252D3D"; radius: 6
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0
                                Text {
                                    text: "TEMP"
                                    color: textGray; font.pixelSize: 10; font.bold: true
                                    Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                                }
                                RowLayout {
                                    spacing: 2
                                    Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                                    Text {
                                        text: sysMonitor.cpuTemp.toFixed(1)
                                        color: sysMonitor.cpuTemp > 65.0 ? "#FF3366" : "#FFaa00"
                                        font.pixelSize: 20; font.bold: true
                                    }
                                    Text {
                                        text: "°C"
                                        color: textGray; font.pixelSize: 12; font.bold: true
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 4
                                    }
                                }
                            }
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: themeBorder }

                // 3. 算法开关
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 15
                    Text { text: "PROCESSING MODULES"; color: textGray; font.pixelSize: 12; font.bold: true }

                    Switch { text: "YOLOv8 Object Detection"; checked: true }
                    Switch { text: "HSV Color Filtering"; checked: true }
                }

                // 4. HSV 双头滑块调节区
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 15
                    Text { text: "HSV CALIBRATION"; color: textGray; font.pixelSize: 12; font.bold: true }

                    // H (Hue)
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "HUE"; color: "white"; Layout.preferredWidth: 40; font.pixelSize: 12; font.bold: true }
                        RangeSlider {
                            id: hSlider; from: 0; to: 180; first.value: 30; second.value: 150; Layout.fillWidth: true
                        }
                        Text { text: Math.round(hSlider.first.value) + " - " + Math.round(hSlider.second.value); color: themeCyan; font.bold: true; Layout.preferredWidth: 55; horizontalAlignment: Text.AlignRight }
                    }

                    // S (Saturation)
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "SAT"; color: "white"; Layout.preferredWidth: 40; font.pixelSize: 12; font.bold: true }
                        RangeSlider {
                            id: sSlider; from: 0; to: 255; first.value: 43; second.value: 255; Layout.fillWidth: true
                        }
                        Text { text: Math.round(sSlider.first.value) + " - " + Math.round(sSlider.second.value); color: themeCyan; font.bold: true; Layout.preferredWidth: 55; horizontalAlignment: Text.AlignRight }
                    }

                    // V (Value)
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "VAL"; color: "white"; Layout.preferredWidth: 40; font.pixelSize: 12; font.bold: true }
                        RangeSlider {
                            id: vSlider; from: 0; to: 255; first.value: 46; second.value: 255; Layout.fillWidth: true
                        }
                        Text { text: Math.round(vSlider.first.value) + " - " + Math.round(vSlider.second.value); color: themeCyan; font.bold: true; Layout.preferredWidth: 55; horizontalAlignment: Text.AlignRight }
                    }
                }

                Item { Layout.fillHeight: true } // 弹性占位

                // 5. 底部连接按钮
                Button {
                    text: "CONNECT VIDEO STREAM"
                    Layout.fillWidth: true
                    height: 50
                    font.bold: true
                    font.pixelSize: 14
                    font.letterSpacing: 1

                    background: Rectangle {
                        color: parent.down ? "#00CC7D" : themeCyan
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#0B0E14"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}