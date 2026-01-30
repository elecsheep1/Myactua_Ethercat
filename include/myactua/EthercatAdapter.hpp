#pragma once
#include "myactua/MotorTypes.hpp"

namespace myactua{

class EthercatAdapter
{
public:
    virtual ~EthercatAdapter() = default;
    virtual bool init(const char* ifname) = 0;      //初始化网口和ethercat网络
    // 物理层收发：整个网络执行一次
    virtual void receivePhysical() = 0; 
    virtual void sendPhysical() = 0;
    // 逻辑层读写：每个电机对象调用一次（仅操作内存）
    virtual void send(int index, const TxPDO& pdo) = 0;     //加&-引用，告诉系统直接去读我原本的内存，节省内存
    virtual RxPDO receive(int index) = 0;
    virtual bool isConfigured(int index) = 0;                //检查电机是否已经配置完成
};
}