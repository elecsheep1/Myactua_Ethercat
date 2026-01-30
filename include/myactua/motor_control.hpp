#pragma once
#include <vector>
#include <memory>
#include <cstdio>
#include "myactua/MotorTypes.hpp"
#include "myactua/EthercatAdapter.hpp"
#include "myactua/CiA402.hpp"


namespace myactua{

/*
* 电机运行状态
*/
enum class MotorStep {
    IDLE,           // 待机
    ENABLING,       // 使能中
    RUNNING,        // 已进入 Operation Enabled，可接收运动指令
    FAULT,          // 故障
    MODE_SWITCHING  // 模式切换中
};

class MYACTUA {
public:
    struct MotorState{
        int slave_index;
        ControlMode target_mode;
        MotorStep step;
        TxPDO tx;
        RxPDO rx;

        MotorState(int index)
            : slave_index(index), target_mode(ControlMode::NONE), step(MotorStep::IDLE), tx({}), rx({}) {}
    };

    /**
     * @param adapter 已经初始化好的 EthercatAdapterIGH 指针
     * @param num_motors 从站数量
     */
    MYACTUA(std::shared_ptr<EthercatAdapter> adapter,int num_motors);

    /**
     * 周期更新函数
     * @param setvalues 目标值数组，长度与从站数量相同
    */
    void update(const std::vector<double>& setvalues);

    void setMode(ControlMode mode,int slave_index);

    /**
     * 连接与初始化网络
    */
    bool connect(const char* ifname);

private:
    std::shared_ptr<EthercatAdapter> _adapter;
    std::vector<MotorState> _motors;

    void processSingleMotor(MotorState& motor, double setvalue);
};

} // namespace myactua