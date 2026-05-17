#pragma once

#include <iostream>
#include <memory>
#include <functional>
#include <cstdint>
#include "hal_ICamera.hpp"

namespace hal {

/**
 * @brief 编码器支持的输出编码格式
 * @note 类似于 OpenCV cv::VideoWriter 的 FourCC
 */
enum class CodecType {
    UNKNOWN = 0,
    H264,       // H.264 / AVC
    H265,       // H.265 / HEVC
    MJPEG       // Motion JPEG
};

/**
 * @brief 编码后的视频帧类型 (NALU 类型)
 */
enum class FrameType {
    UNKNOWN = 0,
    I_FRAME,    // 关键帧 (IDR)
    P_FRAME,    // 向前预测帧
    B_FRAME     // 双向预测帧
};

/**
 * @brief 编码后数据包 (Packet) 封装结构体
 * 包含压缩后的码流数据、字节大小以及时间戳等元数据
 */
struct EncodedPacket {
    std::shared_ptr<uint8_t> data = nullptr; // 指向编码后码流的首地址，生命周期由 shared_ptr 自动管理
    uint32_t size = 0;                       // 编码后数据的有效字节大小
    FrameType type = FrameType::UNKNOWN;     // 当前帧的类型 (I/P/B)
    uint64_t pts = 0;                        // 显示时间戳 (Presentation Time Stamp)，单位通常为微秒或毫秒
    
    // 确保数据包有效
    bool isValid() const { return data != nullptr && size > 0; }
};

/**
 * @brief 视频硬件编码抽象接口类
 */
class IEncoder {
public:
    virtual ~IEncoder() = default;

    /**
     * @brief 初始化并打开硬件编码器 (参考 cv::VideoWriter::open)
     * @param codec 期望输出的编码格式 (H264/H265)
     * @param width 视频宽度 (必须与输入的 VideoFrame 一致)
     * @param height 视频高度 (必须与输入的 VideoFrame 一致)
     * @param fps 目标帧率，用于内部码率控制 (Rate Control)
     * @param bitrate_kbps 目标码率 (单位: kbps)，0 表示使用默认值
     * @param in_format 输入图像的格式，硬件编码器通常对格式有严格要求 (如 X5/RK 平台常要求 NV12)
     * @return true 成功，false 失败
     */
    virtual bool open(CodecType codec, 
                      uint32_t width, 
                      uint32_t height, 
                      int fps, 
                      uint32_t bitrate_kbps = 2000, 
                      PixelFormat in_format = PixelFormat::NV12) = 0;

    /**
     * @brief 关闭编码器，释放底层硬件资源
     */
    virtual void close() = 0;

    /**
     * @brief 同步编码一帧图像 (类似 OpenCV 的 cv::VideoWriter::write)
     * @param frame 需要被编码的原始数据帧 (从 ICamera 获取的数据)
     * @param timeout_ms 超时时间
     * @return EncodedPacket 对象。调用方通过 packet.isValid() 判断是否编码成功。
     * @note 输入的 frame 和输出的 packet 都使用智能指针，避免内存拷贝，底层零拷贝(Zero-copy)友好。
     */
    virtual EncodedPacket encode(const VideoFrame& frame, int timeout_ms = 2000) = 0;

    /**
     * @brief 异步编码回调函数类型
     * @note 针对高性能硬件，通常采用异步回调模式。送入图像后立刻返回，编码完成后触发此回调。
     */
    using EncodeCallback = std::function<void(const EncodedPacket& packet)>;

    /**
     * @brief 设置异步编码的数据回调
     * @param cb 回调函数。如果设置了回调函数，encode() 可能会变成非阻塞模式。
     */
    virtual void setCallback(EncodeCallback cb) = 0;

    /**
     * @brief 强制请求下一个生成的帧为 I 帧 (关键帧)
     * @note 这是硬件编码器的高级 API，当直播推流网络波动、或有新客户端接入时，通常需要调用此接口
     * @return true 成功，false 失败
     */
    virtual bool forceKeyFrame() = 0;

    /**
     * @brief 获取当前编码器状态 (类似 cv::VideoWriter::isOpened)
     * @return 编码器是否已准备好接收数据
     */
    virtual bool isOpened() const = 0;
};

} // namespace hal