#include "systemmonitor.h"
#include <QDebug>
#include <QTimer>

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject{parent}
{
    m_socket = new QTcpSocket(this);

    // 连接 Socket 数据接收信号槽
    connect(m_socket, &QTcpSocket::readyRead, this, &SystemMonitor::onReadyRead);

    // 处理网络错误
    connect(m_socket, &QTcpSocket::errorOccurred, this, &SystemMonitor::onSocketError);
    
    // 处理断开连接
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        qDebug() << "Disconnected from RDK X5. Trying to reconnect...";
        
        // [新增] 推送真实断开日志到 QML 界面
        emit newSysLog("WARN", "NETWORK", "Disconnected from RDK X5. Reconnecting in 3s...");
        
        QTimer::singleShot(3000, this, &SystemMonitor::connectToServer); // 3秒后重连
    });

    // 启动首次连接
    connectToServer();
}

void SystemMonitor::connectToServer()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Connecting to 192.168.127.10:8888 ...";
        
        // [新增] 推送真实的连接尝试日志到 QML 界面
        emit newSysLog("INFO", "NETWORK", "Attempting TCP connection to 192.168.127.10:8888...");
        
        // 保持你正确的 IP 地址
        m_socket->connectToHost("192.168.127.10", 8888);
    }
}

void SystemMonitor::onReadyRead()
{
    while (m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine().trimmed();
        if (line.isEmpty()) continue;

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();

            // 1. 处理分辨率 (配合板端新增的 cam_width 和 cam_height)
            if (obj.contains("cam_width") && obj.contains("cam_height")) {
                int w = obj["cam_width"].toInt();
                int h = obj["cam_height"].toInt();
                QString newRes = QString("%1x%2").arg(w).arg(h);
                if (m_resolution != newRes) {
                    m_resolution = newRes;
                    emit resolutionChanged();
                }
            }

            // 2. 处理 FPS (配合板端发送的 camera_fps)
            if (obj.contains("camera_fps")) {
                float rawFps = obj["camera_fps"].toDouble();
                float roundedFps = qRound(rawFps * 100) / 100.0f;
                if (qAbs(m_fps - roundedFps) > 0.001f) {
                    m_fps = roundedFps;
                    emit fpsChanged();
                }
            }

            // 3. 处理 CPU 温度
            if (obj.contains("cpu_temperature_c")) {
                double rawTemp = obj["cpu_temperature_c"].toDouble();
                double roundedTemp = qRound(rawTemp * 10) / 10.0; // 结果如 42.9
                if (qAbs(m_cpuTemp - roundedTemp) > 0.01) {
                    m_cpuTemp = roundedTemp;
                    emit cpuTempChanged();
                }
            }

            // 4. 处理 CPU 使用率
            if (obj.contains("cpu_usage_percent")) {
                // 服务端发过来的是浮点数，比如 2.5432，这里转 double 后再 qRound 变成整数
                int newCpuUtil = qRound(obj["cpu_usage_percent"].toDouble());
                if (m_cpuUsage != newCpuUtil) {
                    m_cpuUsage = newCpuUtil;
                    emit cpuUsageChanged();
                }
            }

            // 5. 处理 BPU 使用率
            if (obj.contains("bpu_usage_percent")) {
                int newBpuUtil = qRound(obj["bpu_usage_percent"].toDouble());
                if (m_bpuUsage != newBpuUtil) {
                    m_bpuUsage = newBpuUtil;
                    emit bpuUsageChanged();
                }
            }
        }
    }
}

void SystemMonitor::onSocketError()
{
    qWarning() << "Socket Error:" << m_socket->errorString();
    
    // [新增] 将底层的 Socket 错误（如 Connection Refused）推送到 QML 面板显示
    emit newSysLog("ERROR", "SOCKET", m_socket->errorString());
}