#pragma once

#include <string>

class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    /**
     * @brief 获取系统状态并打包为 JSON 字符串
     * @return std::string JSON 格式的字符串
     * @note 外部调用时，两次调用之间需要有时间间隔，否则 CPU 占有率计算将不准确。
     */
    std::string getSystemStatusJson();

private:
    // 记录上一次的 CPU 状态数据，用于计算两次调用之间的增量百分比
    unsigned long long last_total_user_;
    unsigned long long last_total_user_low_;
    unsigned long long last_total_sys_;
    unsigned long long last_total_idle_;

    void initCpuData();
    float getCpuUsage();
    float getCpuTemperature();
    int getBpuUsage();
};