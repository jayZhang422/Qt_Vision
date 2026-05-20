#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QQmlEngine>
#include <QTcpSocket>
#include <QTimer> // [新增]
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

public:
    // [修改] 构造函数接受 IP、端口和重连时间
    explicit SystemMonitor(QString host, int port, int reconnectMs, QObject *parent = nullptr);
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
    void onReadyRead();
    void onSocketError();
    void connectToServer();

private:
    float m_fps = 0.0f;
    QString m_resolution = "1920x1080";
    double m_cpuTemp = 0.0;
    int m_cpuUsage = 0;
    int m_bpuUsage = 0;

    QTcpSocket *m_socket;
    QTimer *m_reconnectTimer; // [新增]
    QString m_host;
    int m_port;
    int m_reconnectMs;
};

#endif // SYSTEMMONITOR_H