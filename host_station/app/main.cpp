#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <gst/gst.h>
#include <yaml-cpp/yaml.h> // [新增] YAML 支持
#include <iostream>

#include "systemmonitor.h"
#include "paramclient.h"     // [新增]
#include "gstvideoreceiver.h"

int main(int argc, char *argv[])
{
    // 如果没有通过命令行传参，默认读取同级目录的 config.host.yaml
    std::string configPath = (argc > 1) ? argv[1] : "config.yaml";

    gst_init(&argc, &argv);
    std::cout << "[Main] GStreamer engine initialized." << std::endl;

    QGuiApplication app(argc, argv);

    // 1. 读取 YAML 配置
    QString edge_host = "192.168.127.10";
    int status_port = 8888;
    int control_port = 8890;
    int reconnect_ms = 3000;

    try {
        YAML::Node config = YAML::LoadFile(configPath);
        edge_host = QString::fromStdString(config["network"]["edge_host"].as<std::string>());
        status_port = config["network"]["status_port"].as<int>();
        control_port = config["network"]["control_port"].as<int>();
        reconnect_ms = config["network"]["reconnect_ms"].as<int>();
        std::cout << "[Main] Loaded config from " << configPath << std::endl;
    } catch (const YAML::Exception& e) {
        std::cerr << "[Main] Config load failed: " << e.what() << ". Using default IPs." << std::endl;
    }

    // 2. 实例化双通道通信类
    SystemMonitor myMonitor(edge_host, status_port, reconnect_ms);
    ParamClient myParamClient(edge_host, control_port, reconnect_ms);

    QQmlApplicationEngine engine;
    qmlRegisterType<GstVideoReceiver>("CustomVideo", 1, 0, "GstVideoReceiver");

    // 3. 将 C++ 对象注册给 QML
    engine.rootContext()->setContextProperty("sysMonitor", &myMonitor);
    engine.rootContext()->setContextProperty("paramClient", &myParamClient);
    
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
        
    engine.loadFromModule("host_station", "Main");

    return QCoreApplication::exec();
}