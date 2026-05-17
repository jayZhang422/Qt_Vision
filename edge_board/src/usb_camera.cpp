#include "usb_camera.hpp"

namespace hal {

UsbCamera::UsbCamera(const std::string& device_path) 
    : m_device_path(device_path), m_use_path(true) {
}

UsbCamera::UsbCamera(int device_id) 
    : m_device_id(device_id), m_use_path(false) {
}

UsbCamera::~UsbCamera() {
    close(); // 遵守 RAII 原则
}

bool UsbCamera::open(uint32_t width, uint32_t height, int fps) {
    std::lock_guard<std::mutex> lock(m_mtx);

    if (m_is_opened) {
        return true;
    }

    // 在 Linux 环境下，强烈建议指定 cv::CAP_V4L2，避免 OpenCV 错误调用 GStreamer 后台
    bool ret = false;
    if (m_use_path) {
        ret = m_cap.open(m_device_path, cv::CAP_V4L2);
    } else {
        ret = m_cap.open(m_device_id, cv::CAP_V4L2);
    }

    if (!ret) {
        std::cerr << "[UsbCamera] Error: Failed to open camera device!" << std::endl;
        return false;
    }

    // 设置分辨率和帧率
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    if (fps > 0) {
        m_cap.set(cv::CAP_PROP_FPS, fps);
    }

    // 强烈建议：将获取格式强制设为 BGR (防止某些奇怪的摄像头吐出 MJPEG 或 YUYV 导致后续处理错乱)
    // m_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G')); // 如果 USB 摄像头带硬解可以打开这行

    m_is_opened = true;
    return true;
}

void UsbCamera::close() {
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_is_opened) {
        m_cap.release();
        m_is_opened = false;
    }
}

bool UsbCamera::isOpened() const {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_is_opened;
}

VideoFrame UsbCamera::getFrame(int timeout_ms) {
    VideoFrame vframe;

    // OpenCV 的 read 通常是阻塞的，它不直接支持 timeout_ms 参数。
    // 在这套简单的 HAL 中，我们忽略 timeout 参数，依赖底层 V4L2 驱动的默认超时(通常是 2 秒)
    
    cv::Mat frame;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (!m_is_opened) {
            return vframe;
        }
        m_cap.read(frame); // 抓图
    }

    if (frame.empty()) {
        std::cerr << "[UsbCamera] Warning: Captured empty frame!" << std::endl;
        return vframe;
    }

    // 填充 VideoFrame 元数据
    vframe.width = frame.cols;
    vframe.height = frame.rows;
    vframe.stride = frame.step; // cv::Mat 自带 step 属性，完美对应 stride
    vframe.buffer_size = frame.total() * frame.elemSize();
    vframe.format = PixelFormat::BGR888; // OpenCV read 出来的默认就是 BGR 三通道

    // =========================================================================
    // 【神级 RAII 零拷贝设计】
    // OpenCV 的 cv::Mat 内部有一个引用计数器。如果我们直接把 frame.data 塞给 shared_ptr，
    // 当这行代码结束，`frame` 局部变量销毁，内存会被直接释放，导致 shared_ptr 拿到野指针。
    // 
    // 解决方法：利用 shared_ptr 的 Custom Deleter 机制，并【值捕获 (Capture by value)】cv::Mat。
    // 当我们写 [m = frame] 时，C++ 会复制一个 cv::Mat 放入闭包，这会触发 cv::Mat 的引用计数 +1。
    // 这样只要 shared_ptr 还活着，闭包里的 m 就活着，底层图像内存就不会被销毁！
    // 当 shared_ptr 最后一个拷贝被销毁时，Lambda 被销毁，m 被销毁，cv::Mat 引用计数归零，内存安全释放。
    // 全程 0 次 memcpy，绝对安全，毫无内存泄漏可能！
    // =========================================================================
    vframe.data = std::shared_ptr<uint8_t>(frame.data, [m = frame](uint8_t*) {
        // 什么都不用做。
        // 当这个 Lambda 表达式生命周期结束时，
        // 捕获的 'm' 发生析构，自动减少 OpenCV 的引用计数。
    });

    return vframe;
}

} // namespace hal