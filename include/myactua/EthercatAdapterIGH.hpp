#pragma once
#include "myactua/EthercatAdapter.hpp"
#include <ecrt.h>
#include <vector>
#include <thread>
#include <atomic>

#define NUM_SLAVES 2

namespace myactua {

class EthercatAdapterIGH : public EthercatAdapter {
private:
    struct SlaveOffsets
    {
        unsigned int off_ctrl_word;     // 0x6040:00
        unsigned int off_target_pos;    // 0x607A:00
        unsigned int off_target_vel;    // 0x60FF:00
        unsigned int off_target_torque; // 0x6071:00
        unsigned int off_max_torque;    // 0x6072:00
        unsigned int off_mode_of_op;    // 0x6060:00
        unsigned int off_reserved_tx;   // 0x5FFE:00
        unsigned int off_status_word;   // 0x6041:00
        unsigned int off_pos;           // 0x6064:00
        unsigned int off_vel;           // 0x606C:00
        unsigned int off_torque;        // 0x6077:00
        unsigned int off_error;         // 0x603F:00
        unsigned int off_mode_disp;     // 0x6061:00
        unsigned int off_reserved_rx;   // 0x5FFE:00
    };

    // 定义映射模板
    static ec_pdo_entry_info_t device_pdo_entries[];
    static ec_pdo_info_t device_pdos[];
    static ec_sync_info_t device_syncs[];

    ec_master_t *master = NULL;
    ec_master_state_t master_state = {};

    ec_domain_t *domain1 = NULL;
    ec_domain_state_t domain1_state = {};

    uint8_t *domain1_pd = nullptr; 

    ec_slave_config_t *sc[NUM_SLAVES] = {0}; 
    ec_slave_config_state_t sc_state[NUM_SLAVES] = {};

    unsigned int sync_ref_counter = 0;
    bool is_initialized = false;
    
    std::vector<SlaveOffsets> slave_offsets;
    std::thread rt_thread;             // 独立的发包线程
    std::atomic<bool> keep_running;    // 线程运行标志
    void rt_loop();                    // 线程循环函数

public:
    EthercatAdapterIGH();
    ~EthercatAdapterIGH() override;

    bool init(const char* ifname) override;
    void send(int index, const TxPDO& pdo) override;
    RxPDO receive(int index) override;

    void receivePhysical() override;
    void sendPhysical() override;
    bool isConfigured(int index) override;
};

}