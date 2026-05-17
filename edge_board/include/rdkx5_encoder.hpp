#pragma once

#include "hal/hal_Encoder.hpp"
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <cstdlib>



namespace hal {

class RDKX5Encoder : public IEncoder {
public:
    RDKX5Encoder() = default;
    
    // 【补丁】满足 RAII，对象销毁时自动关闭硬件，归还内存
    ~RDKX5Encoder() override { 
        close(); 
    }

    bool open(CodecType codec, 
              uint32_t width, 
              uint32_t height, 
              int fps, 
              uint32_t bitrate_kbps = 2000, 
              PixelFormat in_format = PixelFormat::NV12) override;

    void close() override;

    EncodedPacket encode(const VideoFrame& frame, int timeout_ms = 2000) override;

    void setCallback(EncodeCallback cb) override;

    bool forceKeyFrame() override;

    bool isOpened() const override;

private:
    int32_t mapCodecType(CodecType codec);

private:
    void* m_enc_obj = nullptr;
    bool m_is_opened = false;
    mutable std::mutex m_state_mtx; 

    int32_t m_chn = 0;              
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_max_stream_size = 0; 

    // 零拷贝缓冲池设计
    struct BufferPool {
        std::queue<uint8_t*> free_buffers;
        std::mutex mtx;
        std::condition_variable cv;

        // 【补丁】析构函数：释放队列中残留的堆内存，防止裸指针内存泄漏
        ~BufferPool() {
            while (!free_buffers.empty()) {
                uint8_t* p = free_buffers.front();
                free_buffers.pop();
                if (p) {
                    std::free(p);
                }
            }
        }
    };
    
    std::shared_ptr<BufferPool> m_pool;
    static constexpr int POOL_CAPACITY = 5; // 码流缓冲池容量
};

} // namespace hal