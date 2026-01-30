#pragma once
#include <cstdint>

namespace myactua {

constexpr uint32_t TX_CONTROL_WORD_OFFSET = 0;  // 2 bytes
constexpr uint32_t TX_TARGET_POS_OFFSET   = 2;  // 4 bytes
constexpr uint32_t TX_TARGET_VEL_OFFSET   = 6;  // 4 bytes
constexpr uint32_t TX_TARGET_TORQUE_OFFSET= 10; // 2 bytes
constexpr uint32_t TX_MAX_TORQUE_OFFSET   = 12; // 2 bytes
constexpr uint32_t TX_MODE_OF_OP_OFFSET   = 14; // 1 bytes   
constexpr uint32_t TX_RESERVED_OFFSET     = 15; // 1 bytes

constexpr uint32_t TX_PDO_SIZE = 16;

constexpr uint32_t RX_STATUS_WORD_OFFSET  = 0;   // 2 bytes
constexpr uint32_t RX_POS_OFFSET          = 2;   // 4 bytes
constexpr uint32_t RX_VEL_OFFSET          = 6;   // 4 bytes
constexpr uint32_t RX_TORQUE_OFFSET       = 10;  // 2 bytes
constexpr uint32_t RX_ERROR_OFFSET        = 12;  // 2 bytes
constexpr uint32_t RX_MODE_DISP_OFFSET    = 14;  // 1 bytes
constexpr uint32_t RX_RESERVED_OFFSET     = 15;  // 1 bytes

constexpr uint32_t RX_PDO_SIZE = 16;

#pragma pack(push, 1)
struct TxPDO
{
    uint16_t control_word;      // 0x6040
    int32_t target_pos;         // 0x607A
    int32_t target_vel;         // 0x60FF
    int16_t target_torque;      // 0x6071
    uint16_t max_torque;        // 0x6072
    int8_t mode_of_op;          // 0x6060
    uint8_t reserved;           // 0x5FFE (1字节填充)
}__attribute__((packed));

struct RxPDO
{
    uint16_t status_word;       // 0x6041
    int32_t pos;                // 0x6064
    int32_t vel;                // 0x606C
    int16_t torque;             // 0x6077
    uint16_t error;             // 0x603F
    int8_t mode_disp;           // 0x6061
    uint8_t reserved;           // 0x5FFE (1字节填充)
}__attribute__((packed));
#pragma pack(pop)

static_assert(sizeof(TxPDO) == myactua::TX_PDO_SIZE);
static_assert(sizeof(RxPDO) == myactua::RX_PDO_SIZE);

struct SlaveMap{
    uint8_t* tx;
    uint8_t* rx;
};

enum class ControlMode : int8_t{
    NONE = 0,
    CSP = 8,
    CSV = 9,
    CST = 10
};

}// namespace myactua