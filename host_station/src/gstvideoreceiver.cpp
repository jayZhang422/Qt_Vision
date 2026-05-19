#include "gstvideoreceiver.h"
#include <QDebug>

GstVideoReceiver::GstVideoReceiver(QObject *parent)
    : QObject{parent}
{
    // 重要：GStreamer 的回调是在独立的串流线程中触发的。
    // 我们必须使用 Qt::QueuedConnection 将视频帧安全地传递到 Qt 的主 GUI 线程。
    connect(this, &GstVideoReceiver::newFrameReady, 
            this, &GstVideoReceiver::renderFrame, 
            Qt::QueuedConnection);
}

GstVideoReceiver::~GstVideoReceiver()
{
    stop();
}

void GstVideoReceiver::setVideoSink(QVideoSink *sink)
{
    if (m_videoSink != sink) {
        m_videoSink = sink;
        emit videoSinkChanged();
    }
}

void GstVideoReceiver::start()
{
    if (m_pipeline) {
        stop(); // 如果已经在运行，先停止
    }

    // =========================================================================
    // 【核心秘诀】：GStreamer 管道构建
    // 使用 videoconvert 强制将解码后的帧转换为 BGRA 格式，因为 Qt 的 QVideoFrame 最喜欢这种格式。
    // appsink 中配置 drop=true 和 max-buffers=1 是实现超低延迟的灵魂所在！
    // =========================================================================
    QString pipelineStr = "udpsrc port=8888 caps=\"application/x-rtp, media=video, clock-rate=90000, encoding-name=H264\" ! "
                          "rtph264depay ! h264parse ! avdec_h264 ! "
                          "videoconvert ! video/x-raw,format=BGRA ! "
                          "appsink name=mysink drop=true max-buffers=1 emit-signals=true sync=false";

    GError *error = nullptr;
    m_pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

    if (error) {
        qWarning() << "[GStreamer] Pipeline parsing error:" << error->message;
        emit pipelineError(QString::fromUtf8(error->message));
        g_clear_error(&error);
        return;
    }

    // 获取 appsink 元素并挂载回调函数
    m_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
    if (m_appsink) {
        GstAppSinkCallbacks callbacks = {};
        callbacks.new_sample = &GstVideoReceiver::onNewSample;
        // 将 this 指针作为 userData 传进去，以便在静态函数中触发信号
        gst_app_sink_set_callbacks(GST_APP_SINK(m_appsink), &callbacks, this, nullptr);
    }

    // 启动管道
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    qDebug() << "[GStreamer] Video pipeline started successfully.";
}

void GstVideoReceiver::stop()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        
        if (m_appsink) {
            gst_object_unref(m_appsink);
            m_appsink = nullptr;
        }
        qDebug() << "[GStreamer] Video pipeline stopped.";
    }
}

// 供主线程调用的槽函数：直接把帧丢给 QML 的 VideoOutput
void GstVideoReceiver::renderFrame(const QVideoFrame &frame)
{
    if (m_videoSink) {
        m_videoSink->setVideoFrame(frame);
    }
}

// 【底层高频回调】：每收到一帧画面就会执行这里（运行在 GStreamer 线程）
GstFlowReturn GstVideoReceiver::onNewSample(GstAppSink *sink, gpointer userData)
{
    GstVideoReceiver *receiver = static_cast<GstVideoReceiver*>(userData);
    
    // 从 sink 中拉取最新的一帧
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_ERROR;

    // 解析画面的宽高
    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *s = gst_caps_get_structure(caps, 0);
    int width = 0, height = 0;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    // 获取视频数据
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    // 创建 Qt 的视频帧对象 (格式必须与 pipeline 中的 BGRA 对应)
    QVideoFrameFormat format(QSize(width, height), QVideoFrameFormat::Format_BGRA8888);
    QVideoFrame frame(format);

    // 将底层内存拷贝进 QVideoFrame 确保安全
    if (frame.map(QVideoFrame::WriteOnly)) {
        memcpy(frame.bits(0), map.data, map.size);
        frame.unmap();
        
        // 抛出信号，通知主线程去渲染
        emit receiver->newFrameReady(frame);
    }

    // 释放 GStreamer 内存资源
    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}