#include "myactua/EthercatAdapterSOEM.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <vector>  
#include <time.h>



namespace myactua {

EthercatAdapterSOEM::EthercatAdapterSOEM() {
    // 关键：清空 context。因为内部成员是数组/实体，memset 后它们就是初始状态。
    std::memset(&context, 0, sizeof(ecx_contextt));
    is_initialized = false;
    keep_running = false;
}

bool EthercatAdapterSOEM::waitForState(ecx_contextt& ctx, uint16_t slave, uint16_t state, int timeout_ms)
{
    int retry = timeout_ms / 10;
    while (retry--) {
        ecx_readstate(&ctx);
        if (ctx.slavelist[slave].state == state)
            return true;
        usleep(10000);
    }
    return false;
}


bool EthercatAdapterSOEM::init(const char* ifname)
{
    if (!initSocket(ifname)) return false;
    if (!scanSlaves()) return false;
    if (!configPDO()) 
    {
        std::cerr << "无法进入SAFEOP\n";
        return false;
    }
    if (!configDC()) return false;
    if (!requestOP()) 
    {    
        std::cerr << "无法进入OP\n";
        return false;
    }
    std::cerr << "成功进入OP\n";
    is_initialized = true;
    // keep_running = true;
    // rt_thread = std::thread(&EthercatAdapterSOEM::rt_loop, this);
    return true;
}

// 网卡初始化
bool EthercatAdapterSOEM::initSocket(const char* ifname)
{
    if (ecx_init(&context, ifname) <= 0) {
        std::cerr << "ecx_init failed\n";
        return false;
    }
    return true;
}

// 扫描从站
bool EthercatAdapterSOEM::scanSlaves()
{
    if (ecx_config_init(&context) <= 0) {
        std::cerr << "No slaves found\n";
        return false;
    }
    std::cout << "[OK] 发现从站数量: " << context.slavecount << std::endl;
    return true;
}

// 绑定PDO内存
bool EthercatAdapterSOEM::bindPDOMemory()
{
    slave_map.resize(context.slavecount + 1); // 从站索引从1开始，0为主站

    for (uint16_t i = 1; i <= context.slavecount; i++) {
        slave_map[i].tx = context.slavelist[i].outputs;
        slave_map[i].rx = context.slavelist[i].inputs;

        if (!slave_map[i].tx || !slave_map[i].rx) {
            std::cerr << "Slave " << i << " PDO memory not mapped\n";
            return false;
        }
    }
    return true;
}

// PDO映射 + 请求进入SAFEOP
bool EthercatAdapterSOEM::configPDO()
{
    int iomap_size = ecx_config_map_group(&context, IOmap, 0);
    std::cout << "[OK] IOmap size = " << iomap_size << " bytes\n";
    for (int i = 1; i <= context.slavecount; i++) {
        printf("从站 %d PDO数量: %d\n",
            i, context.slavelist[i].Ibytes);
    }

    if (!bindPDOMemory()) return false;

    context.slavelist[0].state = EC_STATE_SAFE_OP;
    ecx_writestate(&context, 0);

    return waitForState(context, 0, EC_STATE_SAFE_OP, EC_TIMEOUTRET * 4);
}

// DC时钟配置
bool EthercatAdapterSOEM::configDC()
{
    ecx_configdc(&context);

    uint32_t cycle = 1000000;
    for (uint16_t i = 1; i <= context.slavecount; i++) {
        ecx_dcsync0(&context, i, TRUE, cycle, 0);
    }
    return true;
}

// 请求op
bool EthercatAdapterSOEM::requestOP()
{
    keep_running = true;
    rt_thread = std::thread(&EthercatAdapterSOEM::rt_loop, this);

    context.slavelist[0].state = EC_STATE_OPERATIONAL;
    ecx_writestate(&context, 0);

    return waitForState(context, 1, EC_STATE_OPERATIONAL, 3000);
}

void EthercatAdapterSOEM::check_ethercat_state(int wc) 
{
    printf("Working Counter: %d\n", wc);

    for (int i = 1; i <= context.slavecount; i++) {
        printf("[Slave %d] AL Status: 0x%02X\n",
            i, context.slavelist[i].ALstatuscode);
    }
}



void EthercatAdapterSOEM::rt_loop() {
    struct timespec ts;
    int64_t cycletime = 1000000; // 1ms = 1,000,000 ns
    
    // 获取当前时间作为起点
    clock_gettime(CLOCK_MONOTONIC, &ts);

    while (keep_running) {
        // 计算下一个周期的时间点
        ts.tv_nsec += cycletime;
        while (ts.tv_nsec >= 1000000000) {
            ts.tv_nsec -= 1000000000;
            ts.tv_sec++;
        }

        // 精确休眠到下一个周期
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);

        // 执行物理收发
        int wc;
        wc = ecx_send_processdata(&context);
        ecx_receive_processdata(&context, EC_TIMEOUTRET);

        ecx_dcsync01(&context, 1, TRUE, (uint32)cycletime, 0, 0);

        // 检查AL 和 wc
        // check_ethercat_state(wc);
    }
}


// 物理接收：从网卡拿数据填入 slavelist[i].inputs
void EthercatAdapterSOEM::receivePhysical() {
    
}

// 物理发送：将 slavelist[i].outputs 里的数据打包发给网卡
void EthercatAdapterSOEM::sendPhysical() {
    
}

// // 逻辑发送：只拷贝数据，不发包
// void EthercatAdapterSOEM::send(int index, const TxPDO& pdo) {
//     if(index > 0 && index <= context.slavecount) {
//         uint8_t* ptr = context.slavelist[index].outputs;
//         if(!ptr) return;

//         //  --- 打印全部发送字节 (十六进制) ---
//         // --- 限制打印频率 ---
//         static int print_counter = 0;
//         print_counter++;

//         if(print_counter >= 500) { // 假设循环是 1ms，这里大约 0.5 秒打印一次
//             // 假设打印前 20 个字节，足以覆盖 RxPDO 的所有内容
//             printf("Slave %d Tx Raw:", index);
//             for(int i = 0; i < 16; i++) {
//                 printf(" %02X", ptr[i]);
//             }
//             printf("\n");
//             print_counter = 0; // 重置计数器
//         }
        
//         // printf(" | Total IOmap: 33 bytes\n");
//         // ------------------------------------

//         // Tx 的顺序通常是对称的，保持从 0 开始写入
//         *((uint16_t*)(ptr + 0)) = pdo.control_word;      
//         *((int32_t*)(ptr + 2))  = pdo.target_pos;        
//         *((int32_t*)(ptr + 6))  = pdo.target_vel;        
//         *((int16_t*)(ptr + 10)) = pdo.target_torque;     
//         *((uint16_t*)(ptr + 12)) = pdo.max_torque;       
//         *((int8_t*)(ptr + 14))  = pdo.mode_of_op;        
//         // *((int8_t*)(ptr + 15)) = pdo.mode_of_op;
//     }
// }

void EthercatAdapterSOEM::send(int index, const TxPDO& pdo)
{
    if (index <= 0 || index > context.slavecount)
    return;

    uint8_t* base = slave_map[index].tx;

    std::memcpy(base + myactua::TX_CONTROL_WORD_OFFSET,  &pdo.control_word,  sizeof(pdo.control_word));
    std::memcpy(base + myactua::TX_TARGET_POS_OFFSET,    &pdo.target_pos,    sizeof(pdo.target_pos));
    std::memcpy(base + myactua::TX_TARGET_VEL_OFFSET,    &pdo.target_vel,    sizeof(pdo.target_vel));
    std::memcpy(base + myactua::TX_TARGET_TORQUE_OFFSET, &pdo.target_torque, sizeof(pdo.target_torque));
    std::memcpy(base + myactua::TX_MAX_TORQUE_OFFSET,    &pdo.max_torque,    sizeof(pdo.max_torque));
    std::memcpy(base + myactua::TX_MODE_OF_OP_OFFSET,    &pdo.mode_of_op,    sizeof(pdo.mode_of_op));

    //  --- 打印全部发送字节 (十六进制) ---
    // --- 限制打印频率 ---
    static int print_counter = 0;
    print_counter++;

    if (print_counter >= 500)
    { // 假设循环是 1ms，这里大约 0.5 秒打印一次
        // 假设打印前 20 个字节，足以覆盖 RxPDO 的所有内容
        printf("Slave %d Tx Raw:", index);
        for (int i = 0; i < 16; i++)
        {
            printf(" %02X", base[i]);
        }
        printf("\n");
        print_counter = 0; // 重置计数器
    }
}


// // 逻辑接收：手动按字节读取反馈
// RxPDO EthercatAdapterSOEM::receive(int index) {
//     RxPDO pdo = {};
//     if(index > 0 && index <= context.slavecount) {
//         uint8_t* ptr = context.slavelist[index].inputs;
//         if(!ptr) return pdo;

//         // 2. --- 打印全部接收字节 (十六进制) ---
//         // --- 限制打印频率 ---
//         static int print_counter = 0;
//         print_counter++;

//         if(print_counter >= 500) { // 假设循环是 1ms，这里大约 0.5 秒打印一次
//             // 假设打印前 20 个字节，足以覆盖 RxPDO 的所有内容
//             printf("Slave %d Rx Raw:", index);
//             for(int i = 0; i < 20; i++) {
//                 printf(" %02X", ptr[i]);
//             }
//             printf("\n");
//             print_counter = 0; // 重置计数器
//         }
        
//         // printf(" | Total IOmap: 33 bytes\n");
//         // ------------------------------------

//         // 根据你打印的 [31 12 00 00...]，StatusWord 就在第 0 位
//         pdo.status_word = *((uint16_t*)(ptr + 0));       // 读到 0x1231
//         pdo.pos         = *((int32_t*)(ptr + 2));        
//         pdo.vel         = *((int32_t*)(ptr + 6));        
//         pdo.torque      = *((int16_t*)(ptr + 10));       
//         pdo.error       = *((uint16_t*)(ptr + 12));      
//         pdo.mode_disp   = *((int8_t*)(ptr + 14));        // 模式位就在这里
//     }
//     return pdo;
// }

RxPDO EthercatAdapterSOEM::receive(int index)
{

    RxPDO pdo{};
    uint8_t* base = slave_map[index].rx;

    std::memcpy(&pdo.status_word, base + myactua::RX_STATUS_WORD_OFFSET, sizeof(pdo.status_word));
    std::memcpy(&pdo.pos,         base + myactua::RX_POS_OFFSET,    sizeof(pdo.pos));
    std::memcpy(&pdo.vel,         base + myactua::RX_VEL_OFFSET,    sizeof(pdo.vel));
    std::memcpy(&pdo.torque,      base + myactua::RX_TORQUE_OFFSET, sizeof(pdo.torque));
    std::memcpy(&pdo.error,       base + myactua::RX_ERROR_OFFSET, sizeof(pdo.error));
    std::memcpy(&pdo.mode_disp,   base + myactua::RX_MODE_DISP_OFFSET,  sizeof(pdo.mode_disp));

    // 2. --- 打印全部接收字节 (十六进制) ---
    // --- 限制打印频率 ---
    static int print_counter = 0;
    print_counter++;

    if (print_counter >= 500)
    { // 假设循环是 1ms，这里大约 0.5 秒打印一次
        // 假设打印前 20 个字节，足以覆盖 RxPDO 的所有内容
        printf("Slave %d Rx Raw:", index);
        for (int i = 0; i < 20; i++)
        {
            printf(" %02X", base[i]);
        }
        printf("\n");
        print_counter = 0; // 重置计数器
    }

    return pdo;
}


bool EthercatAdapterSOEM::isConfigured(int index)
{
    if (!is_initialized) return false;

    if (context.slavelist[index].state != EC_STATE_OPERATIONAL)
        return false;

    // 至少保证 inputs / outputs 可用
    return context.slavelist[index].inputs &&
           context.slavelist[index].outputs;
}


EthercatAdapterSOEM::~EthercatAdapterSOEM() {
    keep_running = false;
    if (rt_thread.joinable())
        rt_thread.join();
    
    if (is_initialized) {
        context.slavelist[0].state = EC_STATE_INIT;
        ecx_writestate(&context, 0);
        usleep(10000);  // 给从站反应时间
        ecx_close(&context);
    }
}

} // namespace myactua