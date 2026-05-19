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

        SetCurrentThreadAffinity({4});

        int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == listen_socket) {
            std::cerr << "[Monitor] Socket creation failed!" << std::endl;
            return;
        }

        // 端口复用，防止重启时端口被占用
        int opt = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in server;
        std::memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(INADDR_ANY);
        server.sin_port = htons(8888);

        if (-1 == bind(listen_socket, (struct sockaddr*)&server, sizeof(server))) {
            std::cerr << "[Monitor] Bind failed!" << std::endl;
            close(listen_socket);
            return;         
        }

        if (-1 == listen(listen_socket, 1)) { // backlog 设为 1，因为我们是单线程处理单客户端
            std::cerr << "[Monitor] Listen failed!" << std::endl;
            close(listen_socket);
            return; 
        }

        std::cout << "[Monitor] Background thread listening on port 8888..." << std::endl;

        // 外层循环：负责不断接收新的客户端连接
        for (;;) {
            int socket_client = accept(listen_socket, NULL, NULL);
            if (-1 == socket_client) continue;

            std::cout << "[Monitor] Client connected!" << std::endl;

            // 实例化监控器对象（此时读取初始基准数据）
            SystemMonitor monitor;

            // 内层循环：负责给当前连接的客户端持续打包发送 JSON
            while (true) {
                // 1秒采样周期
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                // 获取并打包 JSON
                std::string json_data = monitor.getSystemStatusJson() + "\n";

                // 发送数据 (必须使用 MSG_NOSIGNAL，防止客户端断开导致服务端崩溃)
                ssize_t bytes_sent = send(socket_client, json_data.c_str(), json_data.length(), MSG_NOSIGNAL);

                // 如果发送失败（客户端主动断开或网络异常），退出内层循环
                if (bytes_sent <= 0) {
                    std::cout << "[Monitor] Client disconnected. Waiting for new connection..." << std::endl;
                    break; 
                }
            }

            close(socket_client);
        }
        close(listen_socket);
    }
#endif


void Vision_thread()
{
    // ============================================================================
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
// ============================================================================
    SetCurrentThreadAffinity({1,2,3,5});

   hal::UsbCamera cap(0);
   uint32_t width = 1280 ;
   uint32_t height = 720 ;
   int fps = 45 ;

   if(!cap.open(width, height,fps))
   {

    std::cerr<<"[Error] : could not open Camera"<<std::endl;
    return;

   }




#ifdef ENABLE_UDP_TRANSFORM
   hal::RDKX5Encoder encoder;
   GstUdpStreamer streamer;

   uint32_t aligned_w = hal::utils::get_16byte_aligned_stride(width);
   uint32_t aligned_h = hal::utils::get_2byte_aligned_height(height);

   std::string target_ip = "192.168.127.1";//ipwlan :10.185.77.252
   int target_port = 8888;

   if(!encoder.open(hal::CodecType::H264, width, height,fps,4000,hal::PixelFormat::NV12))
   {
    std::cerr<<"[Error]: Init Encoder failed"<<std::endl;
    return;
   }

   if(!streamer.open(target_ip , target_port))
   {
    std::cerr<<"[Error]: Init UDP streamer"<<std::endl;
    return;
   }
#endif

    while(1)
    {
        auto bgr_frame = cap.getFrame();
        if(!bgr_frame.isValid())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        cv::Mat bgr (bgr_frame.height,bgr_frame.width,CV_8UC3,bgr_frame.data.get(),bgr_frame.stride);

        /*
        快速查看命令：
        gst-launch-1.0 udpsrc port=8888 caps="application/x-rtp, media=video, clock-rate=90000, encoding-name=H264" ! rtph264depay ! h264parse ! avdec_h264 ! autovideosink sync=false
        
        */










#ifdef ENABLE_UDP_TRANSFORM
uint32_t nv12_size = aligned_w * aligned_h * 3 / 2;
        uint8_t* raw_nv12_ptr = static_cast<uint8_t*>(std::malloc(nv12_size));
        if (!raw_nv12_ptr) {
            std::cerr << "[Main] OOM: Failed to allocate NV12 buffer!" << std::endl;
            continue;
        }

        std::shared_ptr<uint8_t> nv12_data(raw_nv12_ptr, [](uint8_t* p) { std::free(p); });

        //此处应该传入的是处理完成的Mat
        hal::utils::convert_bgr_to_nv12_libyuv(bgr, nv12_data.get(), aligned_w, aligned_h);

      
        hal::VideoFrame nv12_frame;
        nv12_frame.width = aligned_w;
        nv12_frame.height = aligned_h;
        nv12_frame.stride = aligned_w;
        nv12_frame.format = hal::PixelFormat::NV12;
        nv12_frame.buffer_size = nv12_size;
        nv12_frame.data = nv12_data; 

      
        auto encoded_packet = encoder.encode(nv12_frame);

        
        if (encoded_packet.size > 0 && encoded_packet.data) {
            streamer.pushStream(encoded_packet.data, encoded_packet.size);
        }
#endif
    }





#ifdef ENABLE_UDP_TRANSFORM
    std::cout << "[Vision] Closing streamer and encoder..." << std::endl;
    streamer.close();
    encoder.close();
#endif
    std::cout << "[Vision] Closing camera..." << std::endl;
    cap.close();
    
    std::cout << "[Vision] Thread exited cleanly." << std::endl;
    
}



