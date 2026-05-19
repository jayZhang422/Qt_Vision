#include <iostream>
#include <thread>
#include <chrono>
#include "global_def.hpp"
#include <atomic>
#include <csignal>
std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\n[Main] Interrupt signal (" << signum << ") received. Exiting...\n";
    g_running = false;
}

#ifdef ENABLE_NETWORK_MONITOR

#include "thread_monitor.hpp"

#endif


int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::cout << "System started." << std::endl;

#ifdef ENABLE_NETWORK_MONITOR
   
    std::thread monitor_bg_thread(NetworkMonitorThread);
    monitor_bg_thread.detach(); 
    std::cout << "Network Monitor mode is ON." << std::endl;
#else
    std::cout << "Network Monitor mode is OFF. Running primitive logic." << std::endl;
#endif
    std::thread vision_thread(Vision_thread);
    vision_thread.detach(); 
   
    while (g_running) {
      
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        
    }

    return 0;
}