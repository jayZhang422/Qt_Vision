#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QQmlEngine>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

class SystemMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(float fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(QString resolution READ resolution NOTIFY resolutionChanged)
    Q_PROPERTY(double cpuTemp READ cpuTemp NOTIFY cpuTempChanged)
    Q_PROPERTY(int cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(int bpuUsage READ bpuUsage NOTIFY bpuUsageChanged)
    // 删除了 ping 相关的 Q_PROPERTY

public:
    explicit SystemMonitor(QObject *parent = nullptr);
    float fps() const { return m_fps; }
    QString resolution() const { return m_resolution; }
    double cpuTemp() const { return m_cpuTemp; }
    int cpuUsage() const { return m_cpuUsage; }
    int bpuUsage() const { return m_bpuUsage; }

signals:
    void fpsChanged();
    void resolutionChanged();
    void cpuTempChanged();
    void cpuUsageChanged();
    void bpuUsageChanged();
    void newSysLog(QString type, QString component, QString message);

private slots:
    void onReadyRead();      // 处理网络接收数据
    void onSocketError();    // 处理网络错误（断开重连等）
    void connectToServer();  // 连接服务器函数

private:
    float m_fps = 0.0f;
    QString m_resolution = "1920x1080";
    double m_cpuTemp = 0.0;
    int m_cpuUsage = 0;
    int m_bpuUsage = 0;

    QTcpSocket *m_socket; // 替换原来的 QTimer
};

#endif // SYSTEMMONITOR_H