#include "myactua/motor_control.hpp"
#include "myactua/EthercatAdapterIGH.hpp" // 或 EthercatAdapterSOEM
#include <iostream>  
#include <string>
#include <memory>    
#include <vector>
#include <unistd.h>

#include <atomic>
struct MotorCommand
{
    std::atomic<myactua::ControlMode> mode{myactua::ControlMode::CSV};
    std::atomic<double> setpoint{0.0};
};
std::vector<std::unique_ptr<MotorCommand>> g_commands;

/**
 * 键盘输入处理函数：在独立线程中运行，用于接收用户指令
 */
void keyboard_listener(int num_motors)
{
    // 打印说明
    std::cout << "\n---控制指令说明---" << std::endl;
    std::cout << "设置模式:m [电机索引] [模式代码(8:CSP,9:CSV,10:CST)]" << std::endl;
    std::cout << "设置数值:v [电机索引] [目标值(单位:RPM/度/电流)]" << std::endl;
    std::cout << "示例: m 0 9  -> 设置电机0为速度模式" << std::endl;
    std::cout << "----------------------\n" << std::endl;

    std::string cmd;
    // 循环读取用户输入
    while(true)
    {
        std::cout << ">>";
        if (!(std::cin >> cmd)) break; // 读取指令类型

        int idx;
        // 读取电机索引
        if(!(std::cin >> idx) || idx < 0 || idx >= num_motors)
        {
            std::cout << "[错误] 无效的电机索引！" << std::endl;
            continue;
        }

        if (cmd == "m") // 处理模式切换指令
        {
            int mode_int;
            if (std::cin >> mode_int)
            {
                // 将输入的整数转换为 ControlMode 枚举
                g_commands[idx]->mode.store(static_cast<myactua::ControlMode>(mode_int));// 原子存储
                std::cout << "[INFO] 电机 " << idx << " 模式已设置为 " << mode_int << std::endl;
            }
        }
        else if(cmd == "v") // 处理数值设定指令
        {
            double val;
            if (std::cin >> val)
            {
                g_commands[idx]->setpoint.store(val); // 原子存储
                std::cout << "[INFO] 电机 " << idx << " 目标值已设置为 " << val << std::endl;
            }
        }
    }
}


int main() 
{
    // 初始化电机数量
    const int num_motors = 2;

    // 初始化全局指令缓冲区
    for (int i = 0; i < num_motors; i++)
    {
        // 为每个电机创建一个独立的原子指令存储空间
        g_commands.push_back(std::make_unique<MotorCommand>());
    }

    // 实例化
    auto adapter = std::make_shared<myactua::EthercatAdapterIGH>();

    // 实例化控制类
    myactua::MYACTUA controller(adapter, num_motors);

    std::cout << "[1/3] 正在初始化网卡: " << std::endl;
    // 连接与配置模式
    if (!controller.connect("enp12s0")) {
        std::cerr << "[错误] 无法连接到 EtherCAT 网络！" << std::endl;
        return -1;
    }
    std::cout << "[2/3] 系统就绪，键盘输入线程已启动。" << std::endl;
    std::vector<double> current_setpoints(num_motors, 0.0);

    std::cout << "[3/3] 进入控制循环" << std::endl;
    std::cout << "按 Ctrl+C 停止运行" << std::endl;    

    // 1. 清屏并设置滚动区域
    // \033[2J: 全屏清除
    // \033[13;r: 设置滚动区域从第 13 行开始（1-12 行变成固定面板）
    printf("\033[2J\033[13;r"); 

    // 2. 将键盘监听线程的光标初始位置定在 13 行之后
    printf("\033[13;1H");

    // 启动键盘监听线程
    // 使用 std::thread 异步运行 keyboard_listener，不阻塞主控制循环
    // num_motors 是 keyboard_listener 的参数
    std::thread input_thread(keyboard_listener, num_motors);
    // 将线程分离，使其在后台自主运行
    input_thread.detach();
    
    // 控制循环
    while (true) {
        for (int i = 0; i < num_motors; i++)
        {
            // 从全局指令缓冲区读取最新的模式（由键盘线程写入）
            myactua::ControlMode target_mode = g_commands[i]->mode.load();
            // 更新模式
            controller.setMode(target_mode, i);
            // 读取目标值
            current_setpoints[i] = g_commands[i]->setpoint.load();
        }
        controller.update(current_setpoints);
        usleep(1000); // 1ms 业务周期
    }
    return 0;
}

