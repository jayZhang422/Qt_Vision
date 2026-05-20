#include "systemmonitor.h"
#include <QDebug>

SystemMonitor::SystemMonitor(QString host, int port, int reconnectMs, QObject *parent)
    : QObject{parent}, m_host(host), m_port(port), m_reconnectMs(reconnectMs)
{
    m_socket = new QTcpSocket(this);
    m_reconnectTimer = new QTimer(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &SystemMonitor::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &SystemMonitor::onSocketError);
    
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        emit newSysLog("INFO", "STATUS", "Status Channel Connected to Edge!");
        m_reconnectTimer->stop(); 
    });

    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        emit newSysLog("WARN", "STATUS", "Status Channel Disconnected. Reconnecting...");
        m_fps = 0; // 离线时清零FPS
        emit fpsChanged();
        if (!m_reconnectTimer->isActive()) {
            m_reconnectTimer->start(m_reconnectMs);
        }
    });

    connect(m_reconnectTimer, &QTimer::timeout, this, &SystemMonitor::connectToServer);
    connectToServer();
}

void SystemMonitor::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_socket->connectToHost(m_host, m_port);
    }
}

void SystemMonitor::onSocketError()
{
    m_socket->abort(); // 必须 abort
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(m_reconnectMs);
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
            // ... [保持你原来解析 fps, resolution, cpuTemp 等五项逻辑不变] ...
            if (obj.contains("cam_width") && obj.contains("cam_height")) {
                int w = obj["cam_width"].toInt();
                int h = obj["cam_height"].toInt();
                QString newRes = QString("%1x%2").arg(w).arg(h);
                if (m_resolution != newRes) { m_resolution = newRes; emit resolutionChanged(); }
            }
            if (obj.contains("camera_fps")) {
                float rawFps = obj["camera_fps"].toDouble();
                float roundedFps = qRound(rawFps * 100) / 100.0f;
                if (qAbs(m_fps - roundedFps) > 0.001f) { m_fps = roundedFps; emit fpsChanged(); }
            }
            if (obj.contains("cpu_temperature_c")) {
                double rawTemp = obj["cpu_temperature_c"].toDouble();
                if (qAbs(m_cpuTemp - rawTemp) > 0.01) { m_cpuTemp = rawTemp; emit cpuTempChanged(); }
            }
            if (obj.contains("cpu_usage_percent")) {
                int newCpuUtil = qRound(obj["cpu_usage_percent"].toDouble());
                if (m_cpuUsage != newCpuUtil) { m_cpuUsage = newCpuUtil; emit cpuUsageChanged(); }
            }
            if (obj.contains("bpu_usage_percent")) {
                int newBpuUtil = qRound(obj["bpu_usage_percent"].toDouble());
                if (m_bpuUsage != newBpuUtil) { m_bpuUsage = newBpuUtil; emit bpuUsageChanged(); }
            }
        }
    }
}