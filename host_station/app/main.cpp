#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <gst/gst.h> // [新增] GStreamer 头文件
#include "systemmonitor.h"
#include "gstvideoreceiver.h" // [新增] 我们的自定义类
#include <iostream>
int main(int argc, char *argv[])
{
    // 初始化 GStreamer 底层引擎
    gst_init(&argc, &argv);
    std::cout << "[Main] GStreamer engine initialized." << std::endl;

    QGuiApplication app(argc, argv);
    SystemMonitor myMonitor;
    QQmlApplicationEngine engine;

    // 将自定义的 C++ 接收器注册给 QML (包名为 CustomVideo 1.0)
    qmlRegisterType<GstVideoReceiver>("CustomVideo", 1, 0, "GstVideoReceiver");

    engine.rootContext()->setContextProperty("sysMonitor", &myMonitor);
    
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
        
    engine.loadFromModule("host_station", "Main");

    return QCoreApplication::exec();
}