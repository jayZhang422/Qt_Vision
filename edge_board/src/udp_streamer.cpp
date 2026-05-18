#include "udp_streamer.hpp"
#include <iostream>

GstUdpStreamer::GstUdpStreamer() {
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }
}

GstUdpStreamer::~GstUdpStreamer() {
    close();
}

bool GstUdpStreamer::open(const std::string& target_ip, int target_port) {
    std::string pipeline_str = 
        "appsrc name=mysrc ! h264parse ! rtph264pay config-interval=1 pt=96 ! "
        "udpsink host=" + target_ip + " port=" + std::to_string(target_port) + " sync=false async=false";

    m_pipeline = gst_parse_launch(pipeline_str.c_str(), nullptr);
    if (!m_pipeline) {
        std::cerr << "[UdpStreamer] Failed to create pipeline!" << std::endl;
        return false;
    }

    m_appsrc = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysrc");
    
    GstCaps* caps = gst_caps_from_string("video/x-h264, stream-format=byte-stream, alignment=au");
    g_object_set(G_OBJECT(m_appsrc), "caps", caps, "format", GST_FORMAT_TIME, nullptr);
    gst_caps_unref(caps);

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    m_is_opened = true;
    return true;
}

void GstUdpStreamer::pushStream(std::shared_ptr<uint8_t> data, size_t size) {
    if (!m_is_opened || !data || size == 0) return;

    // 动态分配 shared_ptr 拷贝供 GStreamer 内部使用
    auto* data_ref = new std::shared_ptr<uint8_t>(data);

    GstBuffer* buffer = gst_buffer_new_wrapped_full(
        GST_MEMORY_FLAG_READONLY,
        data.get(),
        size,
        0,
        size,
        data_ref, 
        [](gpointer user_data) {
            // 网络发送完毕后，销毁这个拷贝，引用计数 -1
            delete static_cast<std::shared_ptr<uint8_t>*>(user_data);
        }
    );

    gst_app_src_push_buffer(GST_APP_SRC(m_appsrc), buffer);
}

void GstUdpStreamer::close() {
    if (m_is_opened && m_pipeline) {
        gst_element_send_event(m_pipeline, gst_event_new_eos());
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        gst_object_unref(m_appsrc);
        m_pipeline = nullptr;
        m_appsrc = nullptr;
        m_is_opened = false;
    }
}