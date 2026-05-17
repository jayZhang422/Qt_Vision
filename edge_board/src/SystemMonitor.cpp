#include "SystemMonitor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <memory>
#include <chrono>

// 需要引入 nlohmann/json 库
#include <nlohmann/json.hpp>

using json = nlohmann::json;

SystemMonitor::SystemMonitor() {
    initCpuData(); // 初始化基准 CPU 数据
}

SystemMonitor::~SystemMonitor() {}

std::string SystemMonitor::getSystemStatusJson() {
    json status;
    status["cpu_usage_percent"] = getCpuUsage();
    status["cpu_temperature_c"] = getCpuTemperature();
    status["bpu_usage_percent"] = getBpuUsage();
    status["timestamp_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // dump() 默认输出压缩的单行 JSON，若需格式化可改为 dump(4)
    return status.dump(); 
}

void SystemMonitor::initCpuData() {
    std::ifstream file("/proc/stat");
    std::string line;
    if (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string cpu_label;
        ss >> cpu_label >> last_total_user_ >> last_total_user_low_ >> last_total_sys_ >> last_total_idle_;
    } else {
        last_total_user_ = last_total_user_low_ = last_total_sys_ = last_total_idle_ = 0;
    }
}

float SystemMonitor::getCpuUsage() {
    std::ifstream file("/proc/stat");
    std::string line;
    if (!std::getline(file, line)) return 0.0f;

    std::istringstream ss(line);
    std::string cpu_label;
    unsigned long long total_user, total_user_low, total_sys, total_idle, total_iowait, total_irq, total_softirq;
    
    ss >> cpu_label >> total_user >> total_user_low >> total_sys >> total_idle >> total_iowait >> total_irq >> total_softirq;

    // 计算两次调用之间的差值
    unsigned long long total_user_diff = total_user - last_total_user_;
    unsigned long long total_user_low_diff = total_user_low - last_total_user_low_;
    unsigned long long total_sys_diff = total_sys - last_total_sys_;
    unsigned long long total_idle_diff = total_idle - last_total_idle_;

    unsigned long long total_active = total_user_diff + total_user_low_diff + total_sys_diff;
    unsigned long long total_all = total_active + total_idle_diff;

    float usage = 0.0f;
    if (total_all != 0) {
        usage = (static_cast<float>(total_active) / static_cast<float>(total_all)) * 100.0f;
    }

    // 更新基准值供下一次计算使用
    last_total_user_ = total_user;
    last_total_user_low_ = total_user_low;
    last_total_sys_ = total_sys;
    last_total_idle_ = total_idle;

    return usage;
}

float SystemMonitor::getCpuTemperature() {
    // 读取新指定的硬件监控节点
    std::ifstream file("/sys/class/hwmon/hwmon0/temp1_input");
    long temp_millidegrees = 0;
    if (file >> temp_millidegrees) {
        // 输出为 39920 格式，除以 1000 转换为摄氏度
        return static_cast<float>(temp_millidegrees) / 1000.0f;
    }
    return 0.0f; // 读取失败返回 0
}

int SystemMonitor::getBpuUsage() {
    int bpu_ratio = 0;
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("sudo hrut_somstatus 2>/dev/null", "r"), pclose);

    if (!pipe) return 0;

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line = buffer.data();
        if (line.find("bpu0:") != std::string::npos) {
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;
            while (iss >> token) {
                tokens.push_back(token);
            }
            if (tokens.size() >= 5) {
                try {
                    bpu_ratio = std::stoi(tokens[4]);
                } catch (...) { /* 解析失败保持 0 */ }
            }
            break; // 找到目标行后停止读取
        }
    }
    return bpu_ratio;
}