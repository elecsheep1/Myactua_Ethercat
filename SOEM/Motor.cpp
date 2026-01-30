#include "myactua/Motor.hpp"
#include <iostream>
#include <cstring>

namespace myactua{

/**
 * 构造函数
 * @param adapter 共享的适配器指针，允许多个电机类实例共用同一个 EtherCAT 网卡
 * @param slave_index 该电机在 EtherCAT 链路中的物理顺序索引（从1开始）
 */
MYACTUA::MYACTUA(std::shared_ptr<EthercatAdapter> adapter, int slave_index) : adapter(adapter), slave_index_(slave_index) {
    tx_ = {};
    rx_ = {};
}


/**
 * 连接与初始化网络
 * 注意：在多电机模式下，通常由主程序或第一个电机对象调用一次
 */
bool MYACTUA::connect(const char* ifname)
{
    if(adapter->init(ifname))
        return true;
    return false;
}


/**
 * 设置控制模式 (CSP/CSV/CST)
 * @param mode 目标控制模式
 */
void MYACTUA::setMode(ControlMode mode)
{
    current_mode_ = mode;
    tx_.mode_of_op = static_cast<int8_t>(mode);
}


/**
 * 核心逻辑处理函数
 * 逻辑：逻辑接收缓存 -> 更新状态机 -> 逻辑写入缓存
 * 注意：此函数不再直接触发物理网卡收发，而是操作适配器内部的内存映射
 */
void MYACTUA::process(double value)
{
    updateRx();

    if (rx_.mode_disp != static_cast<int8_t>(current_mode_)) {
        tx_.control_word = 0x0006; // 强行拉回 Ready to Switch On
        tx_.mode_of_op = static_cast<int8_t>(current_mode_);
        // 在模式不匹配时，保持目标值为当前实际值，防止切换瞬间飞车
        tx_.target_pos = rx_.pos;
        tx_.target_vel = 0;
        tx_.target_torque = 0;
    } 
    else {
        // 3. 模式已匹配，按照 CiA402 状态机正常递进
        tx_.mode_of_op = static_cast<int8_t>(current_mode_);
        updateCiA402();
        
        // 4. 只有在真正 Operation Enabled (0x1237/0x123F 且无故障) 时才更新目标
        updateTarget(value);
    }
    
    // updateMode();
    tx_.mode_of_op = static_cast<int8_t>(current_mode_);
    // tx_.max_torque = 5000;
    // // 临时增加打印，看看读到的是什么
    // printf("Target Mode: %d, Actual Mode (from motor): %d, StatusWord: 0x%04x\n", 
    //         tx_.mode_of_op, rx_.mode_disp, rx_.status_word);

    // updateTarget(value);

    adapter->send(slave_index_, tx_);
}



void MYACTUA::updateRx()
{
    rx_ = adapter->receive(slave_index_);
}

void MYACTUA::updateCiA402()
{
    tx_.control_word = CiA402::getNextControlWord(rx_.status_word);
}

void MYACTUA::updateMode()
{
    // 只有在未使能状态下才允许设置模式
    if (!CiA402::isOperationEnabled(rx_.status_word)) {
        tx_.mode_of_op = static_cast<int8_t>(current_mode_);
        mode_confirmed_ = (rx_.mode_disp == tx_.mode_of_op);
    }
}

void MYACTUA::updateTarget(double value)
{
    if (!CiA402::isOperationEnabled(rx_.status_word))
    {
        tx_.target_pos = rx_.pos;
        tx_.target_vel = 0;
        tx_.target_torque = 0;
        return;
    }

    switch (current_mode_)
    {
        case ControlMode::CSV:
            tx_.target_vel = static_cast<int32_t>((value * 131072.0) / 60.0);
            break;

        case ControlMode::CSP:
            tx_.target_pos = static_cast<int32_t>(value * (65535.0 / 180.0));
            break;

        case ControlMode::CST:
            tx_.target_torque = static_cast<int16_t>(value);
            break;

        default:
            break;
    }
}

} // namespace myactua