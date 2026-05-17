#pragma once

#include <iostream>
#include <memory>
#include <functional>

namespace hal {

/**
 * @brief 图像格式枚举
 */
enum class PixelFormat {
    UNKNOWN = 0,
    NV12,      // 12bit
    BGR888,    // 24bit
};

/**
 * @brief 图像帧封装结构体
 * 包含图像数据指针及必要的元数据，特别是针对硬件对齐的 Stride 参数
 */
struct VideoFrame {
    uint32_t width = 0;         // 逻辑宽度 (如 1920)
    uint32_t height = 0;        // 逻辑高度 (如 1080)
    uint32_t stride = 0;        // 内存对齐步长 (对 X5 来说通常是 16 或 32 的倍数)
    uint32_t buffer_size = 0;   // 整个 Buffer 的字节大小
    PixelFormat format = PixelFormat::UNKNOWN ;     // 当前帧的格式
    std::shared_ptr<uint8_t> data = nullptr;  //指向buffer_size首地址的共享指针
    bool isValid() const { return data != nullptr && buffer_size > 0; } //确保指针不为空
};

/**
 * @brief 摄像头硬件抽象接口类
 */
class ICamera {
public:
    virtual ~ICamera() = default;

    /**
     * @brief 初始化并打开摄像头
     * @param width 期望的分辨率宽
     * @param height 期望的分辨率高
     * @param fps 帧率 (-1 表示使用驱动默认值) ，在部分函数里不需要使用fps参数，例如原生opencv
     * @return true 成功，false 失败
     */
    virtual bool open(uint32_t width, uint32_t height, int fps = -1) = 0;

    /**
     * @brief 关闭摄像头，释放所有相关硬件资源
     */
    virtual void close() = 0;

    /**
     * @brief 取一帧图像
     * @param timeout_ms 超时时间
     * @return VideoFrame 对象。调用方通过 frame.isValid() 判断是否获取成功。
     * @note 不需要手动释放！当 VideoFrame 生命周期结束时，底层资源会自动回收。
     */
    virtual VideoFrame getFrame(int timeout_ms = 2000) = 0;

  

    /**
     * @brief 获取当前摄像头状态
     * @return 是否已准备好可以抓图
     */
    virtual bool isOpened() const = 0;
};

} // namespace hal

