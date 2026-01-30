#include "myactua/motor_control.hpp"
#include "myactua/EthercatAdapterIGH.hpp" // 或 EthercatAdapterSOEM
#include <iostream>  
#include <memory>    
#include <vector>
#include <unistd.h>

int main() {
    // 实例化
    auto adapter = std::make_shared<myactua::EthercatAdapterIGH>();
    myactua::MYACTUA controller(adapter, 1);
    std::cout << "[1/3] 正在初始化网卡: " << std::endl;
    // 连接与配置模式
    if (!controller.connect("enp12s0")) {
        std::cerr << "[错误] 无法连接到 EtherCAT 网络！" << std::endl;
        return -1;
    }
    std::cout << "[2/3] 连接成功，正在设置电机1 CSV 模式..." << std::endl;
    controller.setMode(myactua::ControlMode::CSV, 0);
    controller.setMode(myactua::ControlMode::CSV, 1);
    std::vector<double> setpoints = { 100.0 , -100.0};

    std::cout << "[3/3] 进入控制循环" << std::endl;
    std::cout << "按 Ctrl+C 停止运行" << std::endl;    
    // 控制循环
    while (true) {
        controller.update(setpoints);
        usleep(1000); // 1ms 业务周期
    }
    return 0;
}

