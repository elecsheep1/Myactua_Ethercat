#include "myactua/CiA402.hpp"

namespace myactua{
    
uint16_t CiA402::getNextControlWord(uint16_t sw)
{
    //存在故障发送Fault Reset (Bit 7)
    if (isFault(sw)){return 0x0080;}
    //处于启动锁定状态发送 Shutdown 指令解锁
    if (isSwitchOnDisabled(sw)){return 0x0006;}
    //处于准备就绪状态发送 Switch On 指令
    if (isReadyToSwitchOn(sw)){return 0x0007;}
    //处于伺服待机状态发送 Enable Operation 指令
    if (isSwitchedOn(sw)){return 0x000F;}
    //保持
    if (isOperationEnabled(sw)){return 0x000F;}
    return 0x0006;
}

}