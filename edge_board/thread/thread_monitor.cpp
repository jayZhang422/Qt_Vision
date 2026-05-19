#include "global_def.hpp"
#include "hal/hal_Encoder.hpp"
#include "hal/hal_ICamera.hpp"
#include "usb_camera.hpp"
#include "scheduler.hpp"
#include <atomic>
#include <cstdint>
#include <sys/types.h>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>

// [新增] 全局原子变量，用于跨线程安全共享实时帧率，初始值为 0
std::atomic<int> g_camera_fps{0};

#ifdef ENABLE_UDP_TRANSFORM
#include "rdkx5_encoder.hpp"
#include "udp_streamer.hpp"
#include "yuv_utils.hpp"
#endif

#ifdef ENABLE_NETWORK_MONITOR
    // 只有在宏开启时，才会引入网络和 JSON 相关的头文件
    #include <string.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <cerrno>   
    #include <cstring>
    #include "SystemMonitor.hpp" 
    #include <nlohmann/json.hpp>
   
    // ---------------------------------------------------------
    // 单一网络监控线程：负责监听、接收连接、打包并发送 JSON
    // ---------------------------------------------------------
    void NetworkMonitorThread() {
        // 绑定该线程到 CPU 核心 4，避免与其他重要任务抢占资源
        SetCurrentThreadAffinity({4});
        std::cout << "[Monitor] Thread affinity set to core 4." << std::endl;

        int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == listen_socket) {
            std::cerr << "[Monitor] Socket creation failed! Errno: " << errno << std::endl;
            return;
        }

        // 端口复用，防止程序异常重启时提示“端口被占用”
        int opt = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in server;
        std::memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网卡 IP
        server.sin_port = htons(8888);              // 监听 8888 端口

        if (-1 == bind(listen_socket, (struct sockaddr*)&server, sizeof(server))) {
            std::cerr << "[Monitor] Bind failed on port 8888! Errno: " << errno << std::endl;
            close(listen_socket);
            return;         
        }

        // backlog 设为 1，因为我们是单线程且目前只期望处理一个 PC 端上位机
        if (-1 == listen(listen_socket, 1)) { 
            std::cerr << "[Monitor] Listen failed!" << std::endl;
            close(listen_socket);
            return; 
        }

        std::cout << "[Monitor] Background thread successfully listening on port 8888..." << std::endl;

        // 外层循环：负责不断接收新的客户端连接
        for (;;) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int socket_client = accept(listen_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (-1 == socket_client) {
                std::cerr << "[Monitor] Accept failed, retrying..." << std::endl;
                continue;
            }

            std::cout << "[Monitor] Client connected successfully!" << std::endl;

            // 实例化监控器对象（此时读取初始基准数据）
            SystemMonitor monitor;

            // 内层循环：负责给当前连接的客户端持续打包发送 JSON
            while (true) {
                // 1秒采样周期（避免发包过快阻塞网络或消耗过多 CPU）
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                std::string base_json = monitor.getSystemStatusJson();
                std::string final_json_data;

                try {
                    // 解析原来的 JSON 字符串
                    nlohmann::json j = nlohmann::json::parse(base_json);
                    
                    // [核心逻辑] 增加 fps 字段，从原子变量中安全读取(load)当前最新帧率
                    j["camera_fps"] = g_camera_fps.load(); 
                    
                    // 重新序列化打包，加上换行符以便于客户端按行解析
                    final_json_data = j.dump() + "\n";
                } catch (const std::exception& e) {
                    // 如果原 JSON 解析失败，打印错误并回退到原数据发送
                    std::cerr << "[Monitor] JSON parse error: " << e.what() << std::endl;
                    final_json_data = base_json + "\n";
                }
                
                // [已修复] 之前这里误写成了 json_data.length()，现已修正为 final_json_data.length()
                // 发送数据 (必须使用 MSG_NOSIGNAL，防止客户端异常断开导致服务端触发 SIGPIPE 信号崩溃)
                ssize_t bytes_sent = send(socket_client, final_json_data.c_str(), final_json_data.length(), MSG_NOSIGNAL);

                // 如果发送失败（客户端主动断开或网络异常），退出内层循环
                if (bytes_sent <= 0) {
                    std::cout << "[Monitor] Client disconnected. Waiting for new connection..." << std::endl;
                    break; 
                }
            }

            // 断开连接后清理客户端 socket
            close(socket_client);
        }
        close(listen_socket);
    }
#endif

void Vision_thread()
{

// CAMERA CAPABILITIES REPORT (/dev/video0)
// ============================================================================
//
// --- [0] 'MJPG' (Motion-JPEG, compressed) ---
//  1280 x 1024  @  200.000 fps
//  1280 x 960   @  200.000 fps
//  1280 x 720   @  200.000 fps  
//  1024 x 768   @  200.000 fps
//   960 x 540   @  200.000 fps
//   848 x 480   @  200.000 fps
//   800 x 600   @  200.000 fps
//   640 x 512   @  400.000 fps
//   640 x 480   @  400.000 fps  
//   352 x 288   @  400.000 fps
//   320 x 240   @  400.000 fps
//

// --- [1] 'YUYV' (YUYV 4:2:2) ----------------
//  1280 x 1024  @  100.000 fps
//  1280 x 960   @  100.000 fps
//  1280 x 720   @  100.000 fps
//  1024 x 768   @  120.000 fps
//   960 x 540   @  200.000 fps
//   848 x 480   @  200.000 fps
//   800 x 600   @  200.000 fps
//   640 x 512   @  400.000 fps
//   640 x 480   @  400.000 fps
//   640 x 360   @  400.000 fps
//   352 x 288   @  400.000 fps
//   320 x 240   @  400.000 fps
//
// ============================================================================
    
    // 绑定 Vision 线程到核心 1,2,3,5 以保障图像处理性能
    SetCurrentThreadAffinity({1,2,3,5});
    std::cout << "[Vision] Thread affinity set to cores: 1, 2, 3, 5." << std::endl;

    hal::UsbCamera cap(0);
    uint32_t width = 1280 ;
    uint32_t height = 720 ;
    int fps = 45 ;

    std::cout << "[Vision] Attempting to open camera at " << width << "x" << height << " @" << fps << "fps..." << std::endl;
    if(!cap.open(width, height, fps))
    {
        std::cerr<<"[Error] : could not open Camera /dev/video0"<<std::endl;
        return;
    }
    std::cout << "[Vision] Camera opened successfully." << std::endl;

#ifdef ENABLE_UDP_TRANSFORM
    hal::RDKX5Encoder encoder;
    GstUdpStreamer streamer;

    // 获取内存对齐后的宽高，用于 NV12 格式转换和硬件编码器要求
    uint32_t aligned_w = hal::utils::get_16byte_aligned_stride(width);
    uint32_t aligned_h = hal::utils::get_2byte_aligned_height(height);

    std::string target_ip = "192.168.127.1"; //ipwlan:10.185.77.252
    int target_port = 8888;

    std::cout << "[Vision] Initializing H264 Encoder..." << std::endl;
    if(!encoder.open(hal::CodecType::H264, width, height, fps, 4000, hal::PixelFormat::NV12))
    {
        std::cerr<<"[Error]: Init Encoder failed"<<std::endl;
        return;
    }

    std::cout << "[Vision] Initializing UDP Streamer to " << target_ip << ":" << target_port << "..." << std::endl;
    if(!streamer.open(target_ip, target_port))
    {
        std::cerr<<"[Error]: Init UDP streamer failed"<<std::endl;
        return;
    }

    // [新增] 初始化 FPS 计算所需的时间戳和计数器
    auto fps_start_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    std::cout << "[Vision] Pipeline setup complete. Entering main capture loop." << std::endl;
#endif

    while(1)
    {
        auto bgr_frame = cap.getFrame();
        if(!bgr_frame.isValid())
        {
            // 如果获取失败，短暂休眠防止 CPU 空转
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 用智能指针的数据包装为 cv::Mat（浅拷贝，不耗费大量内存）
        cv::Mat bgr (bgr_frame.height, bgr_frame.width, CV_8UC3, bgr_frame.data.get(), bgr_frame.stride);

        /*
        快速查看命令：
        gst-launch-1.0 udpsrc port=8888 caps="application/x-rtp, media=video, clock-rate=90000, encoding-name=H264" ! rtph264depay ! h264parse ! avdec_h264 ! autovideosink sync=false
        */










        

#ifdef ENABLE_UDP_TRANSFORM

        // --- 实时帧率(FPS)统计逻辑开始 ---
        frame_count++; // 成功捕获并准备处理一帧有效画面，计数器+1
        auto current_time = std::chrono::steady_clock::now();
        // 计算自上次清零后经过了多少毫秒
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - fps_start_time).count();
        
        if (elapsed_ms >= 1000) { // 如果已经过去1秒 (1000ms)
            // 线程安全地(store)更新全局帧率变量，供 NetworkMonitorThread 读取
            g_camera_fps.store(frame_count); 
            
            // 打印当前的实际处理帧率到终端
            std::cout << "[Vision] Current Real-time FPS: " << frame_count << " fps" << std::endl;
            
            // 重置计数器和起始时间，开启下一秒的统计
            frame_count = 0;
            fps_start_time = current_time;
        }
        // --- 实时帧率(FPS)统计逻辑结束 ---

        // 计算 NV12 格式所需的总内存大小 (Y分量大小 + UV分量大小)
        uint32_t nv12_size = aligned_w * aligned_h * 3 / 2;
        uint8_t* raw_nv12_ptr = static_cast<uint8_t*>(std::malloc(nv12_size));
        
        if (!raw_nv12_ptr) {
            std::cerr << "[Vision] OOM Error: Failed to allocate NV12 buffer!" << std::endl;
            continue;
        }

        // 使用 shared_ptr 管理内存，确保作用域结束时自动 free，防止内存泄漏
        std::shared_ptr<uint8_t> nv12_data(raw_nv12_ptr, [](uint8_t* p) { std::free(p); });

        // 调用底层库将 BGR 转为 NV12 (用于硬件编码)
        //此处传入处理完成的Mat
        hal::utils::convert_bgr_to_nv12_libyuv(bgr, nv12_data.get(), aligned_w, aligned_h);

        // 构造视频帧结构体，准备送入硬件编码器
        hal::VideoFrame nv12_frame;
        nv12_frame.width = aligned_w;
        nv12_frame.height = aligned_h;
        nv12_frame.stride = aligned_w;
        nv12_frame.format = hal::PixelFormat::NV12;
        nv12_frame.buffer_size = nv12_size;
        nv12_frame.data = nv12_data; 

        // 执行硬件 H.264 编码
        auto encoded_packet = encoder.encode(nv12_frame);

        // 如果编码成功产生有效数据流，通过 UDP 发送出去
        if (encoded_packet.size > 0 && encoded_packet.data) {
            streamer.pushStream(encoded_packet.data, encoded_packet.size);
        }
#endif
    }

#ifdef ENABLE_UDP_TRANSFORM
    std::cout << "[Vision] Closing UDP streamer and H264 encoder..." << std::endl;
    streamer.close();
    encoder.close();
#endif
    std::cout << "[Vision] Closing camera..." << std::endl;
    cap.close();
    
    std::cout << "[Vision] Thread exited cleanly." << std::endl;
}