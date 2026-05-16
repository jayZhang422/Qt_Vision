#include <pthread.h>
#include <sched.h>
#include <vector>
#include <iostream>

//不允许传入核心0
void SetCurrentThreadAffinity(const std::vector<int>& core_ids) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int id : core_ids) {
        CPU_SET(id, &cpuset);
    }
    
    // pthread_self() 获取当前线程自己的句柄
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        perror("Error setting affinity");
    }
}