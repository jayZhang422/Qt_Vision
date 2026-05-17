#include "rdkx5_encoder.hpp"
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <unistd.h> // for usleep
#include "sp_codec.h"
#include "sp_vio.h"
#include "sp_sys.h"
extern "C" {
    void* sp_init_encoder_module();
    void sp_release_encoder_module(void* obj);
    int32_t sp_start_encode(void* obj, int32_t chn, int32_t type, int32_t width, int32_t height, int32_t bits);
    int32_t sp_stop_encode(void* obj);
    int32_t sp_encoder_set_frame(void* obj, char* frame_buffer, int32_t size);
    int32_t sp_encoder_get_stream(void* obj, char* stream_buffer);
}

namespace hal {

int32_t RDKX5Encoder::mapCodecType(CodecType codec) {
    switch (codec) {
        case CodecType::H264:  return SP_ENCODER_H264;
        case CodecType::H265:  return SP_ENCODER_H265;
        case CodecType::MJPEG: return SP_ENCODER_MJPEG;
        default: return SP_ENCODER_H264; 
    }
}

bool RDKX5Encoder::open(CodecType codec, uint32_t width, uint32_t height, int fps, uint32_t bitrate_kbps, PixelFormat in_format) {
    printf("\n[DEBUG] === Enter RDKX5Encoder::open() ===\n");
    fflush(stdout);

    std::lock_guard<std::mutex> lock(m_state_mtx);

    if (m_is_opened) {
        printf("[DEBUG] Encoder is already opened.\n");
        return true;
    }

    if (in_format != PixelFormat::NV12) {
        std::cerr << "[RDKX5Encoder] Error: Hardware encoder only supports NV12 input!\n";
        return false;
    }

    m_width = width;
    m_height = height;
    
    // 初始化编码模块
    m_enc_obj = sp_init_encoder_module();
    if (!m_enc_obj) {
        std::cerr << "[RDKX5Encoder] Error: sp_init_encoder_module failed!\n";
        return false;
    }

    // 开启编码通道
    int32_t sp_type = mapCodecType(codec);
    m_chn = 0; // 默认使用通道 0
    
    int32_t err = sp_start_encode(m_enc_obj, m_chn, sp_type, m_width, m_height, bitrate_kbps);
    if (err != 0) {
        std::cerr << "[RDKX5Encoder] Error: sp_start_encode failed!\n";
        sp_release_encoder_module(m_enc_obj);
        m_enc_obj = nullptr;
        return false;
    }

    // 分配码流缓冲池
    m_max_stream_size = m_width * m_height; 
    m_pool = std::make_shared<BufferPool>();

    for (int i = 0; i < POOL_CAPACITY; ++i) {
        uint8_t* buffer = static_cast<uint8_t*>(std::malloc(m_max_stream_size));
        if (!buffer) {
            printf("[FATAL] Encoder malloc failed at buffer %d!\n", i);
        }
        m_pool->free_buffers.push(buffer);
    }

    m_is_opened = true;
    printf("[DEBUG] === Exit RDKX5Encoder::open() successfully ===\n\n");
    return true;
}

void RDKX5Encoder::close() {
    std::lock_guard<std::mutex> lock(m_state_mtx);

    if (!m_is_opened) {
        return;
    }

    m_is_opened = false;

    if (m_enc_obj) {
        printf("[DEBUG] Closing Encoder hardware...\n");
        sp_stop_encode(m_enc_obj);
        sp_release_encoder_module(m_enc_obj);
        m_enc_obj = nullptr;
    }

    // 触发 m_pool 的析构，进而执行 BufferPool 的析构函数安全释放内存
    m_pool.reset();
}

EncodedPacket RDKX5Encoder::encode(const VideoFrame& frame, int timeout_ms) {
    EncodedPacket packet;

    if (!m_is_opened || !m_pool || !frame.isValid()) {
        std::cerr << "[RDKX5Encoder] Invalid state or frame.\n";
        return packet;
    }

    // 1. 送入原始 NV12 图像进行编码 (零拷贝：直接使用 frame 内部指针)
    int32_t set_err = sp_encoder_set_frame(m_enc_obj, reinterpret_cast<char*>(frame.data.get()), frame.buffer_size);
    if (set_err != 0) {
        std::cerr << "[RDKX5Encoder] Error: sp_encoder_set_frame failed!\n";
        return packet;
    }

    // 2. 从内存池取出一个空闲的 Buffer 用于存放码流
    uint8_t* stream_buffer = nullptr;
    {
        std::unique_lock<std::mutex> lock(m_pool->mtx);
        bool success = m_pool->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() {
            return !m_pool->free_buffers.empty();
        });

        if (!success) {
            std::cerr << "[RDKX5Encoder] Warning: encode timeout! Stream buffer pool starvation.\n";
            return packet;
        }

        stream_buffer = m_pool->free_buffers.front();
        m_pool->free_buffers.pop();
    }

    // 3. 【补丁】获取编码好的码流 (带有自旋重试机制，应对非阻塞/高延迟 API)
    int32_t stream_size = 0;
    int retry_count = 0;
    const int max_retries = 3; 

    while (retry_count < max_retries) {
        stream_size = sp_encoder_get_stream(m_enc_obj, reinterpret_cast<char*>(stream_buffer));
        
        if (stream_size > 0) {
            break; // 成功拿到流
        }
        
        retry_count++;
        usleep(2000); // 睡 2 毫秒，等硬件压完
    }
    
    if (stream_size <= 0) {
        // 多次重试依然失败，归还内存到缓冲池
        std::lock_guard<std::mutex> lock(m_pool->mtx);
        m_pool->free_buffers.push(stream_buffer);
        m_pool->cv.notify_one();
        return packet; 
    }

    // 4. 组装输出包
    packet.size = static_cast<uint32_t>(stream_size);
    packet.type = FrameType::UNKNOWN; 

    // 5. 【神级设计】注入 Custom Deleter 闭环控制
    std::shared_ptr<BufferPool> pool_ref = m_pool;
    packet.data = std::shared_ptr<uint8_t>(stream_buffer, [pool_ref](uint8_t* p) {
        if (pool_ref) {
            // UDP 发送完数据包被销毁时，指针会回到这个闭包中，安全入队
            std::lock_guard<std::mutex> lock(pool_ref->mtx);
            pool_ref->free_buffers.push(p);
            pool_ref->cv.notify_one();
        } else {
            // 极低概率：如果在发送期间 Encoder 整个被 close/销毁了，退化为普通 free
            std::free(p);
        }
    });

    return packet;
}

bool RDKX5Encoder::isOpened() const {
    std::lock_guard<std::mutex> lock(m_state_mtx);
    return m_is_opened;
}

void RDKX5Encoder::setCallback(EncodeCallback cb) {
    std::cerr << "[RDKX5Encoder] Async callback is not supported by this sync wrapper yet.\n";
}

bool RDKX5Encoder::forceKeyFrame() {
    return false; // 当前 API 未暴露请求 I 帧的功能
}

} // namespace hal