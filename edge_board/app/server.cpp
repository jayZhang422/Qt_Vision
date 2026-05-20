#include <iostream>
#include <thread>
#include <chrono>
#include "global_def.hpp"
#include <atomic>
#include <csignal>
#include <yaml-cpp/yaml.h>
std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\n[Main] Interrupt signal (" << signum << ") received. Exiting...\n";
    g_running = false;
}

#ifdef ENABLE_NETWORK_MONITOR

#include "thread_monitor.hpp"

#endif


int main(int argc , char** argv) {

    std::string config_path = "/home/sunrise/cpp_server/config/config.yaml";
    if (argc > 1) {
        config_path = argv[1];
    }
    YAML::Node config;
    try {
        config = YAML::LoadFile(config_path);
    } catch (const YAML::Exception& e) {
        std::cerr << "加载配置文件失败: " << e.what() << std::endl;
        return -1;
    }

    // 读取参数，并传给后续线程
    std::string target_ip = config["network"]["target_ip"].as<std::string>();
    int video_port = config["network"]["video_port"].as<int>();
    int control_port = config["network"]["control_port"].as<int>();
    int cam_width = config["camera"]["width"].as<int>();
    int cam_height = config["camera"]["height"].as<int>();
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::cout << "System started." << std::endl;

#ifdef ENABLE_NETWORK_MONITOR
   
    std::thread monitor_bg_thread([=](){
        NetworkMonitorThread(cam_width,cam_height,target_ip,control_port);
    });
    monitor_bg_thread.detach(); 
    std::cout << "Network Monitor mode is ON." << std::endl;
#else
    std::cout << "Network Monitor mode is OFF. Running primitive logic." << std::endl;
#endif
  std::thread vision_thread([=]() {
    Vision_thread(cam_width, cam_height, target_ip, video_port);
});
vision_thread.detach();
   
    while (g_running) {
      
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        
    }

    return 0;
}