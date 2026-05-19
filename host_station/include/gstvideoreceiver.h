#ifndef GSTVIDEORECEIVER_H
#define GSTVIDEORECEIVER_H

#include <QObject>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QtQml/qqml.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

class GstVideoReceiver : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // 这个属性用于在 QML 中绑定 VideoOutput 的 videoSink
    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)

public:
    explicit GstVideoReceiver(QObject *parent = nullptr);
    ~GstVideoReceiver();

    QVideoSink* videoSink() const { return m_videoSink; }
    void setVideoSink(QVideoSink *sink);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void videoSinkChanged();
    // 跨线程传递视频帧的信号
    void newFrameReady(const QVideoFrame &frame);
    void pipelineError(QString msg);

private slots:
    // 在主线程中渲染视频帧
    void renderFrame(const QVideoFrame &frame);

private:
    QVideoSink *m_videoSink = nullptr;
    GstElement *m_pipeline = nullptr;
    GstElement *m_appsink = nullptr;

    // GStreamer appsink 的回调函数（静态函数）
    static GstFlowReturn onNewSample(GstAppSink *sink, gpointer userData);
};

#endif // GSTVIDEORECEIVER_H