#pragma once
#include "myactua/EthercatAdapter.hpp"
#include <thread>
#include <atomic>
#include <vector>  

extern "C" {
    #include "soem/soem.h"
    #include "soem/ec_dc.h"
}

namespace myactua {

class EthercatAdapterSOEM : public EthercatAdapter {
private:
    ecx_contextt context;  
    char IOmap[4096];
    bool is_initialized = false;

    std::thread rt_thread;             // 独立的发包线程
    std::atomic<bool> keep_running;    // 线程运行标志
    void rt_loop();                    // 线程循环函数

    std::vector<SlaveMap> slave_map;

    bool waitForState(ecx_contextt& ctx, uint16_t slave, uint16_t state, int timeout_ms);
    bool initSocket(const char* ifname);
    bool scanSlaves();
    bool bindPDOMemory();
    bool configPDO();
    bool configDC();
    bool requestOP();
    void check_ethercat_state(int wc);
public:
    EthercatAdapterSOEM();
    ~EthercatAdapterSOEM() override;

    bool init(const char* ifname) override;
    void receivePhysical() override;
    void sendPhysical() override;
    void send(int index, const TxPDO& pdo) override;
    RxPDO receive(int index) override;

    bool isConfigured(int index) override;
};

}