#include <iostream>
#include <thread>
#include <chrono>


#define ENABLE_NETWORK_MONITOR 

#ifdef ENABLE_NETWORK_MONITOR

#include "thread_monitor.cpp"

#endif


int main() {
    std::cout << "System started." << std::endl;

#ifdef ENABLE_NETWORK_MONITOR
   
    std::thread monitor_bg_thread(NetworkMonitorThread);
    monitor_bg_thread.detach(); 
    std::cout << "Network Monitor mode is ON." << std::endl;
#else
    std::cout << "Network Monitor mode is OFF. Running primitive logic." << std::endl;
#endif

   
    while (true) {
      
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        
    }

    return 0;
}