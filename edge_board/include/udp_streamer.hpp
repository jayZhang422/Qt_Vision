#pragma once
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <string>
#include <memory>

class GstUdpStreamer {
public:
    GstUdpStreamer();
    ~GstUdpStreamer();

    bool open(const std::string& target_ip, int target_port);
    void close();

    /**
     * @brief 发送码流数据 (完全解耦，不依赖任何自定义结构体)
     * @param data 数据的智能指针，利用 GStreamer 闭包进行生命周期管理 (零拷贝)
     * @param size 数据的有效字节数
     */
    void pushStream(std::shared_ptr<uint8_t> data, size_t size);

private:
    GstElement* m_pipeline = nullptr;
    GstElement* m_appsrc = nullptr;
    bool m_is_opened = false;
};