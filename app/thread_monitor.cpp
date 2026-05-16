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
    #include "scheduler.hpp"
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
