#include "paramclient.h"
#include <QDebug>

ParamClient::ParamClient(QString host, int port, int reconnectMs, QObject *parent)
    : QObject(parent), m_host(host), m_port(port), m_reconnectMs(reconnectMs)
{
    m_socket = new QTcpSocket(this);
    m_reconnectTimer = new QTimer(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &ParamClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ParamClient::onSocketError);

    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        emit newSysLog("INFO", "PARAM", "Control Channel Connected to Edge!");
        m_reconnectTimer->stop(); // 连上就停止重试
    });

    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        emit newSysLog("WARN", "PARAM", "Control Channel Disconnected. Reconnecting...");
        if (!m_reconnectTimer->isActive()) {
            m_reconnectTimer->start(m_reconnectMs);
        }
    });

    connect(m_reconnectTimer, &QTimer::timeout, this, &ParamClient::connectToServer);

    // 初始连接
    connectToServer();
}

void ParamClient::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        emit newSysLog("INFO", "PARAM", QString("Attempting to connect %1:%2...").arg(m_host).arg(m_port));
        m_socket->connectToHost(m_host, m_port);
    }
}

void ParamClient::onSocketError()
{
    m_socket->abort(); // 必须 abort 才能进行下一次连接
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(m_reconnectMs);
    }
}

void ParamClient::setParam(const QString& name, int value)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit newSysLog("WARN", "PARAM", "Cannot set param, control channel offline.");
        return;
    }

    QJsonObject jsonObj;
    jsonObj["type"] = "param_set";
    jsonObj["name"] = name;
    jsonObj["value"] = value;

    QJsonDocument doc(jsonObj);
    QString jsonString = doc.toJson(QJsonDocument::Compact) + "\n";
    
    m_socket->write(jsonString.toUtf8());
}

void ParamClient::onReadyRead()
{
    m_buffer += QString::fromUtf8(m_socket->readAll());
    
    // 按行拆包解析 (JSON Line 协议)
    int newlinePos;
    while ((newlinePos = m_buffer.indexOf('\n')) != -1) {
        QString line = m_buffer.left(newlinePos).trimmed();
        m_buffer.remove(0, newlinePos + 1);

        if (line.isEmpty()) continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj["type"].toString() == "param_ack") {
                QString name = obj["name"].toString();
                int val = obj["value"].toInt();
                bool ok = obj["ok"].toBool();
                QString errorMsg = obj["error"].toString();

                emit paramAckReceived(name, val, ok, errorMsg);
                
                if (ok) {
                    emit newSysLog("INFO", "PARAM", QString("ACK: Set [%1] to %2 OK").arg(name).arg(val));
                } else {
                    emit newSysLog("ERROR", "PARAM", QString("ACK Failed: %1").arg(errorMsg));
                }
            }
        }
    }
}