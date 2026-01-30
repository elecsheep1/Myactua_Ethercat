#include "myactua/EthercatAdapterIGH.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <vector>  
#include <time.h>

#define NSEC_PER_SEC (1000000000L)
#define CLOCK_TO_USE CLOCK_MONOTONIC
#define VID_PID          0x00202008,0x00000000   /*Vendor ID, product code	厂商ID和产品代码*/
#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

namespace myactua {

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
    struct timespec result;

    if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) {
        result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
    } else {
        result.tv_sec = time1.tv_sec + time2.tv_sec;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    }

    return result;
}

// ---定义静态配置模板---
ec_pdo_entry_info_t EthercatAdapterIGH::device_pdo_entries[] = {
    {0x6040, 0x00, 16},
    {0x607a, 0x00, 32},
    {0x60ff, 0x00, 32},
    {0x6071, 0x00, 16},
    {0x6072, 0x00, 16},
    {0x6060, 0x00, 8},
    {0x5ffe, 0x00, 8},  // 填充字节
    {0x6041, 0x00, 16},
    {0x6064, 0x00, 32},
    {0x606c, 0x00, 32},
    {0x6077, 0x00, 16},
    {0x603f, 0x00, 16},
    {0x6061, 0x00, 8},
    {0x5ffe, 0x00, 8},  // 填充字节
};

ec_pdo_info_t EthercatAdapterIGH::device_pdos[] = {
    // RX 主站发送
    {0x1600, 7, &EthercatAdapterIGH::device_pdo_entries[0]},
    // TX 主站接收
    {0x1a00, 7, &EthercatAdapterIGH::device_pdo_entries[7]},
};

ec_sync_info_t EthercatAdapterIGH::device_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 1, &EthercatAdapterIGH::device_pdos[0], EC_WD_ENABLE},
    {3, EC_DIR_INPUT, 1, &EthercatAdapterIGH::device_pdos[1], EC_WD_DISABLE},
    {0xff}
};    


EthercatAdapterIGH::EthercatAdapterIGH() {
    slave_offsets.resize(NUM_SLAVES);
    is_initialized = false;
    keep_running = false;
}    

EthercatAdapterIGH::~EthercatAdapterIGH() {
    keep_running = false;
    if (rt_thread.joinable()) rt_thread.join();
    if (master) ecrt_release_master(master);
}

bool EthercatAdapterIGH::init(const char* ifname) 
{
    master = ecrt_request_master(0);    // 请求主站控制权
    if (!master) 
    {
        std::cerr << "请求主站失败\n";
        return false;
    }

    domain1 = ecrt_master_create_domain(master); // 创建域
    if (!domain1) 
    {
        std::cerr << "创建域失败\n";
        return false;
    }

    // 配置从站实体
    for (uint16_t i = 0; i < NUM_SLAVES; i++) 
    {
        sc[i] = ecrt_master_slave_config(master, 0, i, VID_PID);
        if(!sc[i]) 
        {
            std::cerr << "配置从站 " << i << " 失败\n";
            return false;
        }
        
        if (ecrt_slave_config_pdos(sc[i], EC_END, device_syncs)) 
        {
            std::cerr << "配置从站 " << i << " PDO 失败\n";
            return false;
        }

        ecrt_slave_config_dc(sc[i], 0x0300, 1000000, 4400000, 0, 0); // 配置 DC 时钟

        // 注册 PDO 条目到 Domain
        /*
        ec_pdo_entry_reg_t
        uint16_t alias;       从站别名 (Alias)
        uint16_t position;    从站物理位置 (Position)
        uint32_t vendor_id;   厂家 ID (Vendor ID)
        uint32_t product_code;产品代码 (Product Code)
        uint16_t index;       对象字典索引 (Index)
        uint8_t subindex;     对象字典子索引 (Subindex)
        unsigned int *offset; 偏移量变量的指针 (Pointer to offset variable)
        unsigned int *bit_pos;位偏移指针 (Pointer to bit position)
        */
        ec_pdo_entry_reg_t reg[] ={
            {0, i, VID_PID, 0x6040, 0, &slave_offsets[i].off_ctrl_word},
            {0, i, VID_PID, 0x607A, 0, &slave_offsets[i].off_target_pos},
            {0, i, VID_PID, 0x60FF, 0, &slave_offsets[i].off_target_vel},
            {0, i, VID_PID, 0x6071, 0, &slave_offsets[i].off_target_torque},
            {0, i, VID_PID, 0x6072, 0, &slave_offsets[i].off_max_torque},
            {0, i, VID_PID, 0x6060, 0, &slave_offsets[i].off_mode_of_op},

            {0, i, VID_PID, 0x6041, 0, &slave_offsets[i].off_status_word},
            {0, i, VID_PID, 0x6064, 0, &slave_offsets[i].off_pos},
            {0, i, VID_PID, 0x606C, 0, &slave_offsets[i].off_vel},
            {0, i, VID_PID, 0x6077, 0, &slave_offsets[i].off_torque},
            {0, i, VID_PID, 0x603F, 0, &slave_offsets[i].off_error},
            {0, i, VID_PID, 0x6061, 0, &slave_offsets[i].off_mode_disp},
            {} // 结束标志
        };

        if (ecrt_domain_reg_pdo_entry_list(domain1, reg)) 
        {
            std::cerr << "注册从站 " << i << " PDO 条目失败\n";
            return false;
        }
    }

    if(ecrt_master_activate(master))
    {
        std::cerr << "激活主站失败\n";
        return false;
    }

    if(!(domain1_pd = ecrt_domain_data(domain1))) 
    {
        std::cerr << "获取域数据失败\n";
        return false;
    }

    is_initialized = true;
    keep_running = true;
    rt_thread = std::thread(&EthercatAdapterIGH::rt_loop, this);
    return true;    
}

void EthercatAdapterIGH::rt_loop() 
{
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        perror("设置实时优先级失败");
    }
    struct timespec wakeup_time,time;   // 定义时间结构体
    clock_gettime(CLOCK_TO_USE,&wakeup_time); // 获取当前时间
    // 周期时间
    const struct timespec cycle_time = {0, 1000000}; // 1ms = 1e6 ns
    while (keep_running)
    {
        wakeup_time = timespec_add(wakeup_time, cycle_time); // 1ms周期
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeup_time, nullptr); // 精确睡眠到下一个周期,TIMER_ABSTIME(绝对时间)
        ecrt_master_application_time(master, TIMESPEC2NS(wakeup_time)); // 更新主站应用时间（用于从站同步插补）
        /*EtherCAT 从站（如电机驱动器）内部有 DC 时钟
        从站根据主站给的应用时间（Application Time）计算插补值
        你每 1ms 发一次目标位置，从站用应用时间计算当前时刻位置*/
        
        ecrt_master_receive(master); // 接收从站 PDO 数据
        ecrt_domain_process(domain1); // 处理域数据

        /*下面是数据处理*/


        if (sync_ref_counter) {
            sync_ref_counter--;
        } else {
            sync_ref_counter = 1; // sync every cycle

            clock_gettime(CLOCK_TO_USE, &time); // 获取当前时间
            ecrt_master_sync_reference_clock_to(master, TIMESPEC2NS(time)); //同步时钟
        }
        ecrt_master_sync_slave_clocks(master); // 同步从站时钟

        ecrt_domain_queue(domain1); // 队列域数据准备发送
        ecrt_master_send(master); // 发送 PDO 数据到从站
    }
}

void EthercatAdapterIGH::send(int index, const TxPDO& pdo)
{
    if(index < 0 || index >= slave_offsets.size()) return;

    SlaveOffsets& off = slave_offsets[index];
    EC_WRITE_U16(domain1_pd + off.off_ctrl_word,    pdo.control_word);
    EC_WRITE_S32(domain1_pd + off.off_target_pos,   pdo.target_pos);
    EC_WRITE_S32(domain1_pd + off.off_target_vel,   pdo.target_vel);
    EC_WRITE_S16(domain1_pd + off.off_target_torque,pdo.target_torque);
    EC_WRITE_U16(domain1_pd + off.off_max_torque,   pdo.max_torque);
    EC_WRITE_S8 (domain1_pd + off.off_mode_of_op,   pdo.mode_of_op);
}

RxPDO EthercatAdapterIGH::receive(int index)
{
    RxPDO pdo = {};
    if(index < 0 || index >= slave_offsets.size()) return pdo;

    SlaveOffsets& off = slave_offsets[index];
    pdo.status_word = EC_READ_U16(domain1_pd + off.off_status_word);
    pdo.pos         = EC_READ_S32(domain1_pd + off.off_pos);
    pdo.vel         = EC_READ_S32(domain1_pd + off.off_vel);
    pdo.torque      = EC_READ_S16(domain1_pd + off.off_torque);
    pdo.error       = EC_READ_U16(domain1_pd + off.off_error);
    pdo.mode_disp   = EC_READ_S8 (domain1_pd + off.off_mode_disp);

    return pdo;
}

void EthercatAdapterIGH::receivePhysical() {
}


void EthercatAdapterIGH::sendPhysical() {
}

// 检查特定从站的配置状态
bool EthercatAdapterIGH::isConfigured(int index) {
    return true;
}


} // namespace myactua