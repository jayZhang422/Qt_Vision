#pragma once
#include <cstdint>
#include "string"

#ifdef ENABLE_NETWORK_MONITOR
void NetworkMonitorThread(uint32_t width = 1280 ,
    uint32_t height = 720 ,
    std::string target_ip = "192.168.127.1",
    int target_port = 8890);
void ParamControlThread(int control_port = 8890);
#endif
void Vision_thread(uint32_t width , uint32_t height , 
    std::string target_ip = "192.168.127.1",int target_port = 8888);