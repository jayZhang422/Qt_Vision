#pragma once

#include <opencv2/opencv.hpp>
#include <libyuv.h>
#include <cstdint>
#include <vector>

namespace hal {
namespace utils {

/**
 * @brief 计算 16-byte 对齐的宽度 (RDK X5 硬件编码要求)
 */
inline uint32_t get_16byte_aligned_stride(uint32_t width) {
    return (width + 15) & ~15;
}

/**
 * @brief 计算 2-byte 对齐的高度 (NV12 格式要求高度为偶数)
 */
inline uint32_t get_2byte_aligned_height(uint32_t height) {
    return (height + 1) & ~1;
}

/**
 * @brief 使用 libyuv 将 OpenCV BGR 图像极速转换为 NV12 格式
 * 严格满足 RDK 硬件编码器的 16 字节对齐要求
 * 
 * @param bgr           [in]  OpenCV 原始 BGR 图像
 * @param nv12_out      [out] 预先分配好的 NV12 目标内存首地址
 * @param aligned_w     [in]  对齐后的物理宽度 (Stride)
 * @param aligned_h     [in]  对齐后的物理高度
 */
inline void convert_bgr_to_nv12_libyuv(const cv::Mat& bgr, uint8_t* nv12_out, 
                                       uint32_t aligned_w, uint32_t aligned_h) {
    if (bgr.empty() || !nv12_out) return;

    // 1. 定位输出的 Y 平面和 UV 平面
    uint32_t y_size = aligned_w * aligned_h;
    uint8_t* nv12_y  = nv12_out;
    uint8_t* nv12_uv = nv12_out + y_size;

    // 2. 利用 std::vector 分配临时 I420 中转内存 (函数结束自动释放，避免内存泄漏)
    uint32_t u_size = (aligned_w / 2) * (aligned_h / 2);
    std::vector<uint8_t> i420_buf(y_size + u_size * 2);
    
    uint8_t* i420_y = i420_buf.data();
    uint8_t* i420_u = i420_y + y_size;
    uint8_t* i420_v = i420_u + u_size;

    // 3. 第一步：BGR888 -> I420
    libyuv::RGB24ToI420(
        bgr.data,             // src_rgb24
        bgr.step[0],          // src_stride_rgb24: 使用 cv::Mat 自带的 step，防止内存不连续
        i420_y,               // dst_y
        aligned_w,            // dst_stride_y
        i420_u,               // dst_u
        aligned_w / 2,        // dst_stride_u
        i420_v,               // dst_v
        aligned_w / 2,        // dst_stride_v
        bgr.cols,             // width: 传入实际逻辑宽度，只转换有效区域！
        bgr.rows              // height: 传入实际逻辑高度
    );

    // 4. 第二步：I420 -> NV12
    libyuv::I420ToNV12(
        i420_y, aligned_w,
        i420_u, aligned_w / 2,
        i420_v, aligned_w / 2,
        nv12_y, aligned_w,    // 目标 NV12 Y 的物理跨度
        nv12_uv, aligned_w,   // 目标 NV12 UV 的物理跨度
        aligned_w, aligned_h  // 进行交织操作的整体宽高
    );
}

} // namespace utils
} // namespace hal