#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "systemmonitor.h"
#include <QQmlContext>
/*
读取：FPS，Resolution ratio， Cpu temperature，Cpu占用率，Bpu占用率，ping延迟，
读写：HSV
开关：yolo，HSV color filter
显示：摄像头画面
*/

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    SystemMonitor myMonitor;
    QQmlApplicationEngine engine;
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
