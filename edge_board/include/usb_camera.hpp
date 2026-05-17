#pragma once

#include "hal/hal_ICamera.hpp" // 包含你定义的 ICamera 和 VideoFrame
#include <opencv2/opencv.hpp>
#include <mutex>
#include <string>

namespace hal {

class UsbCamera : public ICamera {
public:
    /**
     * @brief 构造函数 (通过设备节点路径打开，如 "/dev/video0")
     */
    explicit UsbCamera(const std::string& device_path);

    /**
     * @brief 构造函数 (通过 OpenCV 默认设备 ID 打开，如 0)
     */
    explicit UsbCamera(int device_id = 0);

    /**
     * @brief 析构函数，满足 RAII，自动释放摄像头
     */
    ~UsbCamera() override;

    bool open(uint32_t width, uint32_t height, int fps = -1) override;

    void close() override;

    VideoFrame getFrame(int timeout_ms = 2000) override;

    bool isOpened() const override;

private:
    cv::VideoCapture m_cap;
    mutable std::mutex m_mtx; // 保护设备状态
    bool m_is_opened = false;

    // 记录打开方式
    std::string m_device_path;
    int m_device_id = 0;
    bool m_use_path = false;
};

} // namespace hal