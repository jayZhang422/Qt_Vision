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
#include <unordered_map>
#include <opencv2/opencv.hpp>

// 引入 main 函数中的退出控制标志
extern std::atomic<bool> g_running;

// [新增] 全局原子变量，用于跨线程安全共享实时帧率，初始值为 0
std::atomic<int> g_camera_fps{0};

// =========================================================================
// --- 定义全局原子参数 (供 Vision_thread 和 TCP 调参线程跨线程安全读写) ---
// =========================================================================
std::atomic<int> g_h_min{30};
std::atomic<int> g_h_max{150};
std::atomic<int> g_s_min{43};
std::atomic<int> g_s_max{255};
std::atomic<int> g_v_min{46};
std::atomic<int> g_v_max{255};

// --- 参数注册表结构 ---
struct ParamEntry {
    std::atomic<int>* value;
    int minValue;
    int maxValue;
};

// --- 初始化参数白名单 ---
std::unordered_map<std::string, ParamEntry> g_paramTable = {
    {"h_min", {&g_h_min, 0, 180}},
    {"h_max", {&g_h_max, 0, 180}},
    {"s_min", {&g_s_min, 0, 255}},
    {"s_max", {&g_s_max, 0, 255}},
    {"v_min", {&g_v_min, 0, 255}},
    {"v_max", {&g_v_max, 0, 255}}
};


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
    // 单一网络监控线程：负责监听、接收连接、打包并发送 JSON 状态
    // ---------------------------------------------------------
    void NetworkMonitorThread(uint32_t width = 1280,
                              uint32_t height = 720,
                              std::string target_ip = "192.168.127.1",
                              int target_port = 8888) 
    {
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
        server.sin_port = htons(target_port);       // 监听传入的端口

        if (-1 == bind(listen_socket, (struct sockaddr*)&server, sizeof(server))) {
            std::cerr << "[Monitor] Bind failed on port " << target_port << "! Errno: " << errno << std::endl;
            close(listen_socket);
            return;         
        }

        // backlog 设为 1，因为我们是单线程且目前只期望处理一个 PC 端上位机
        if (-1 == listen(listen_socket, 1)) { 
            std::cerr << "[Monitor] Listen failed!" << std::endl;
            close(listen_socket);
            return; 
        }

        std::cout << "[Monitor] Background thread successfully listening on port " << target_port << "..." << std::endl;

        
        while (g_running.load()) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int socket_client = accept(listen_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (-1 == socket_client) {
                // 如果是收到 Ctrl+C 打断的 accept 阻塞，不再打印错误
                if (g_running.load()) std::cerr << "[Monitor] Accept failed, retrying..." << std::endl;
                continue;
            }

            std::cout << "[Monitor] Client connected successfully!" << std::endl;

            // 实例化监控器对象（此时读取初始基准数据）
            SystemMonitor monitor;

           
            while (g_running.load()) {
                // 1秒采样周期（避免发包过快阻塞网络或消耗过多 CPU）
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

               
                if (!g_running.load()) break;

                std::string base_json = monitor.getSystemStatusJson();
                std::string final_json_data;

                try {
                    // 解析原来的 JSON 字符串
                    nlohmann::json j = nlohmann::json::parse(base_json);
                    
                    // [核心逻辑] 增加 fps 字段，从原子变量中安全读取(load)当前最新帧率
                    j["camera_fps"] = g_camera_fps.load(); 
                    j["cam_width"] = width;  
                    j["cam_height"] = height;  
                    
                    // 重新序列化打包，加上换行符以便于客户端按行解析
                    final_json_data = j.dump() + "\n";
                } catch (const std::exception& e) {
                    // 如果原 JSON 解析失败，打印错误并回退到原数据发送
                    std::cerr << "[Monitor] JSON parse error: " << e.what() << std::endl;
                    final_json_data = base_json + "\n";
                }
                
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

    // ---------------------------------------------------------
    // 新增：TCP 控制参数接收线程 (Param Control Thread)
    // ---------------------------------------------------------
    void ParamControlThread(int control_port = 8890) {
        // 同步绑定该线程到 CPU 核心 4，集中处理网络任务
        SetCurrentThreadAffinity({4});
        std::cout << "[ParamControl] Thread affinity set to core 4. Listening on port " << control_port << "..." << std::endl;

        int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket == -1) {
            std::cerr << "[ParamControl] Socket creation failed!" << std::endl;
            return;
        }

        int opt = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(control_port);

        if (bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            std::cerr << "[ParamControl] Bind failed!" << std::endl;
            close(listen_socket);
            return;
        }

        if (listen(listen_socket, 1) == -1) {
            std::cerr << "[ParamControl] Listen failed!" << std::endl;
            close(listen_socket);
            return;
        }

        while (g_running.load()) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket == -1) {
                if (g_running.load()) std::cerr << "[ParamControl] Accept failed, retrying..." << std::endl;
                continue;
            }

            std::cout << "[ParamControl] Host Control Client connected!" << std::endl;
            
            std::string buffer = "";
            char chunk[1024];

            while (g_running.load()) {
                ssize_t bytes_read = recv(client_socket, chunk, sizeof(chunk), 0);
                
                if (bytes_read <= 0) {
                    std::cout << "[ParamControl] Client disconnected." << std::endl;
                    break; 
                }

                buffer.append(chunk, bytes_read);

                size_t newline_pos;
                while ((newline_pos = buffer.find('\n')) != std::string::npos) {
                    std::string line = buffer.substr(0, newline_pos);
                    buffer.erase(0, newline_pos + 1);

                    // 移除可能带有的 \r
                    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

                    if (line.empty()) continue;

                    try {
                        auto j = nlohmann::json::parse(line);
                        
                        // 只处理 param_set 类型的设定指令
                        if (j.value("type", "") == "param_set") {
                            std::string name = j.value("name", "");
                            int value = j.value("value", 0);
                            
                            bool ok = false;
                            std::string error_msg = "";

                            auto it = g_paramTable.find(name);
                            if (it != g_paramTable.end()) {
                                if (value >= it->second.minValue && value <= it->second.maxValue) {
                                    it->second.value->store(value);
                                    ok = true;
                                    std::cout << "[Param] Updated " << name << " = " << value << std::endl;
                                } else {
                                    error_msg = "Value out of range";
                                }
                            } else {
                                error_msg = "Parameter not found";
                            }

                            // 回执打包
                            nlohmann::json ack;
                            ack["type"] = "param_ack";
                            ack["name"] = name;
                            ack["value"] = value;
                            ack["ok"] = ok;
                            if (!ok) ack["error"] = error_msg;

                            std::string ack_str = ack.dump() + "\n";
                            send(client_socket, ack_str.c_str(), ack_str.length(), MSG_NOSIGNAL);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[ParamControl] JSON parse error: " << e.what() << std::endl;
                    }
                }
            }
            close(client_socket);
        }
        close(listen_socket);
    }
#endif

void Vision_thread(uint32_t width, uint32_t height, 
                   std::string target_ip = "192.168.127.1", int target_port = 8888)
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

    // 初始化 FPS 计算所需的时间戳和计数器
    auto fps_start_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    std::cout << "[Vision] Pipeline setup complete. Entering main capture loop." << std::endl;
#endif

    // 【修改点】：不再是 while(1)，受全局运行标志控制，完美退出
    while(g_running.load())
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

        // -----------------------------------------------------------
        // (你可以像下面这样在算法中直接读取全局参数了)
        // int cur_h_min = g_h_min.load();
        // int cur_h_max = g_h_max.load();
        // ... (在这里放入你的 HSV 二值化算法) ...
        // -----------------------------------------------------------

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