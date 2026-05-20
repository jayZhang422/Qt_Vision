#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>

class ParamClient : public QObject
{
    Q_OBJECT
public:
    explicit ParamClient(QString host, int port, int reconnectMs, QObject *parent = nullptr);

    // 供 QML 调用的下发参数接口
    Q_INVOKABLE void setParam(const QString& name, int value);

signals:
    // 回执信号
    void paramAckReceived(QString name, int value, bool ok, QString errorMsg);
    // 日志信号，复用之前的控制台
    void newSysLog(QString type, QString component, QString message);

private slots:
    void onReadyRead();
    void onSocketError();
    void connectToServer();

private:
    QTcpSocket *m_socket;
    QTimer *m_reconnectTimer;
    QString m_host;
    int m_port;
    int m_reconnectMs;
    QString m_buffer; // 处理粘包和断包
};