#include "myactua/motor_control.hpp"

#define LIMIT(VAL, MIN, MAX) ((VAL)<(MIN)?(MIN):((VAL)>(MAX)?(MAX):(VAL)))

namespace myactua{

MYACTUA::MYACTUA(std::shared_ptr<EthercatAdapter> adapter, int num_motors)
    : _adapter(adapter)
{
    for (int i = 0; i < num_motors; i++) {
        _motors.emplace_back(i); // 从站索引从0开始
    }
}

bool MYACTUA::connect(const char* ifname)
{
    if(_adapter->init(ifname))
        return true;
    return false;
}

void MYACTUA::update(const std::vector<double> &setvalues)
{
    static int print_count = 0; // 静态计数器

    for (size_t i = 0; i < _motors.size(); i++)
    {
        _motors[i].rx = _adapter->receive(_motors[i].slave_index);

        double val = (i < setvalues.size()) ? setvalues[i] : 0.0;
        processSingleMotor(_motors[i], val);

        _adapter->send(_motors[i].slave_index, _motors[i].tx);
    }

    // 打印
    // 每 100 个周期（约 100ms）执行一次界面刷新
    if (++print_count >= 100) {
        print_count = 0; // 重置计数器

        // 保存光标并回到左上角
        printf("\033[s\033[H");

        // 绘制表头
        printf("\n\033[1;36m============================ MOTOR REAL-TIME MONITOR ============================\033[0m\n");
        // ID(6) | STEP(12) | POS(10) | VEL(10) | TORQUE(10) | TARGET(12) | MODE(6)
        printf("%-6s | %-12s | %-10s | %-10s | %-10s | %-12s | %-6s\n", 
            "ID", "STEP", "POS", "VEL", "TORQUE", "TARGET(TX)", "MODE");
        printf("---------------------------------------------------------------------------------\n");

        for (const auto& m : _motors) {
            // 1. 确定颜色字符串 (char*)
            const char* color_code = "\033[32m"; // 绿色
            if (m.step == MotorStep::FAULT) color_code = "\033[31m"; // 红色
            if (m.step == MotorStep::MODE_SWITCHING) color_code = "\033[33m"; // 黄色

            // 2. 根据模式确定当前要观察的发送值
            int32_t current_target = 0;
            if (m.rx.mode_disp == 8)      current_target = m.tx.target_pos;
            else if (m.rx.mode_disp == 9) current_target = m.tx.target_vel;
            else if (m.rx.mode_disp == 10) current_target = (int32_t)m.tx.target_torque;

            // 3. 严格对应的格式化打印
            // 这里的参数顺序必须是：
            // m.slave_index -> %-6d (这里原本是 M %-4d，我们统一格式)
            // color_code    -> %s
            // (int)m.step   -> %-12d
            // m.rx.pos      -> %-10d
            // m.rx.vel      -> %-10d
            // m.rx.torque   -> %-10d
            // current_target-> %-12d
            // m.rx.mode_disp-> %-6d
            printf("M %-4d | %s%-12d\033[0m | %-10d | %-10d | %-10d | %-12d | %-6d\n", 
                m.slave_index,     // 对应 M %-4d
                color_code,        // 对应 %s
                (int)m.step,       // 对应 %-12d
                m.rx.pos,          // 对应 %-10d
                m.rx.vel,          // 对应 %-10d
                m.rx.torque,       // 对应 %-10d
                current_target,    // 对应 %-12d
                (int)m.rx.mode_disp); // 对应 %-6d
        }
        printf("\033[1;36m=================================================================================\033[0m\n");
        
        // 恢复光标到输入行
        printf("\033[u");
        fflush(stdout); 
    }
}

void MYACTUA::processSingleMotor(MotorState& motor, double setvalue)
{
    uint16_t sw = motor.rx.status_word;

    // 检查用户设定的目标模式是否与驱动器当前的模式(6061h)不一致
    if (motor.rx.mode_disp != static_cast<int8_t>(motor.target_mode)) 
    {   
        motor.step = MotorStep::MODE_SWITCHING; // 标记状态为切换中
        motor.tx.control_word = 0x0006; // 强行拉回 Ready to Switch On
        motor.tx.mode_of_op = static_cast<int8_t>(motor.target_mode);
        motor.tx.target_pos = motor.rx.pos;     // 将当前实际位置设为目标位置
        motor.tx.target_vel = 0;                // 速度清零
        motor.tx.target_torque = 0;             // 力矩清零
        return;
    }

    // 故障处理
    if (CiA402::isFault(sw)) 
    {
        motor.step = MotorStep::FAULT;
        motor.tx.control_word = CiA402::getNextControlWord(sw);
        return;
    }

    // 状态机推进
    motor.tx.control_word = CiA402::getNextControlWord(sw);

    // 目标值计算
    if (CiA402::isOperationEnabled(sw)) 
    {   
        motor.step = MotorStep::RUNNING;
        motor.tx.max_torque = 5000; // 设置最大力矩为1000 (根据实际需求调整)
        switch (motor.target_mode) 
        {
            case ControlMode::CSV:
                motor.tx.target_vel = static_cast<int32_t>((setvalue * 131072.0) / 60.0);
                break;
            case ControlMode::CSP:
                motor.tx.target_pos = static_cast<int32_t>(setvalue * (65535.0 / 180.0));
                break;
            case ControlMode::CST:
                motor.tx.target_torque = static_cast<int16_t>(setvalue);
                break;
            default:
                break;
        }
    } 
    else 
    {
        motor.step = MotorStep::ENABLING;
        motor.tx.target_pos = motor.rx.pos;
        motor.tx.target_vel = 0;
        motor.tx.target_torque = 0;
    }
}

void MYACTUA::setMode(ControlMode mode, int slave_index)
{
    if(slave_index >= 0 && slave_index < _motors.size())
    {
        _motors[slave_index].target_mode = mode;
    }
}

}