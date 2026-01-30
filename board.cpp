#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <iomanip>
#include <mutex>
#include "myactua/motor_control.hpp"
#include "myactua/EthercatAdapterIGH.hpp"

using namespace myactua;

// 全局变量，用于线程间通信
std::atomic<ControlMode> g_target_mode(ControlMode::CSV);
std::atomic<double> g_target_value(0.0);
std::atomic<bool> g_running(true);
std::mutex g_display_mutex;

// 格式化打印函数
void printStatus(const MYACTUA::MotorState& state) {
    std::lock_guard<std::mutex> lock(g_display_mutex);
    std::cout << "\r" << std::dec
              << "[模式:" << (int)state.rx.mode_disp << "]"
              << " [状态:0x" << std::hex << std::setw(4) << std::setfill('0') << state.rx.status_word << "]"
              << " [位置:" << std::dec << std::setw(9) << state.rx.pos << "]"
              << " [速度:" << std::setw(7) << state.rx.vel << "]"
              << " [Step:" << (int)state.step << "]"
              << " >> 目标:" << std::fixed << std::setprecision(2) << g_target_value.load()
              << "    " << std::flush;
}

// 后台控制线程：负责高频 EtherCAT 通讯
void controlLoop(std::shared_ptr<MYACTUA> actua) {
    while (g_running) {
        // 更新控制模式
        actua->setMode(g_target_mode.load(), 0);

        // 发送指令并更新状态
        std::vector<double> cmds = { g_target_value.load() };
        actua->update(cmds);

        // 每 10ms 在屏幕上刷新一次简单状态（不换行）
        static int ui_count = 0;
        if (++ui_count >= 10) {
            printStatus(actua->getMotorState(0));
            ui_count = 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main(int argc, char** argv) {
    auto adapter = std::make_shared<EthercatAdapterIGH>();
    auto actua = std::make_shared<MYACTUA>(adapter, 1);

    std::string iface = (argc > 1) ? argv[1] : "enp12s0"; // 可通过命令行传网卡名

    std::cout << "正在连接网卡: " << iface << " ..." << std::endl;
    if (!actua->connect(iface.c_str())) {
        return -1;
    }

    // 启动控制线程
    std::thread rt_thread(controlLoop, actua);

    std::cout << "\n==========================================" << std::endl;
    std::cout << "  MyActuator EtherCAT 交互调试工具" << std::endl;
    std::cout << "  命令列表:" << std::endl;
    std::cout << "  m : 切换模式 (8:CSP, 9:CSV, 10:CST)" << std::endl;
    std::cout << "  v : 设置目标值 (RPM / 角度 / 电流)" << std::endl;
    std::cout << "  s : 紧急归零并停止" << std::endl;
    std::cout << "  q : 退出程序" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    while (g_running) {
        char cmd;
        std::cin >> cmd;

        if (cmd == 'q') {
            g_target_value = 0;
            g_running = false;
        } 
        else if (cmd == 's') {
            g_target_value = 0;
            std::cout << "\n[ACTION] 紧急停止！" << std::endl;
        }
        else if (cmd == 'm') {
            int mode_input;
            std::cout << "\n[INPUT] 请输入模式代码 (8:CSP, 9:CSV, 10:CST): ";
            std::cin >> mode_input;
            
            ControlMode new_mode = static_cast<ControlMode>(mode_input);
            
            // 重要：切换到位置模式前自动对齐，防止跳动
            if (new_mode == ControlMode::CSP) {
                const auto& state = actua->getMotorState(0);
                double current_angle = (double)state.rx.pos * (360.0 / 131072.0);
                g_target_value = current_angle;
                std::cout << "[INFO] 已对齐当前位置角度: " << current_angle << std::endl;
            } else {
                g_target_value = 0;
            }
            
            g_target_mode = new_mode;
            std::cout << "[INFO] 模式已切换为: " << mode_input << std::endl;
        }
        else if (cmd == 'v') {
            double val;
            std::cout << "\n[INPUT] 请输入新的目标值: ";
            std::cin >> val;
            g_target_value = val;
            std::cout << "[INFO] 目标值已更新为: " << val << std::endl;
        }
    }

    std::cout << "\n正在关闭控制线程..." << std::endl;
    if (rt_thread.joinable()) rt_thread.join();
    
    std::cout << "程序安全退出。" << std::endl;
    return 0;
}