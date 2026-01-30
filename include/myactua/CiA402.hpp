#pragma once 
#include <cstdint>

namespace myactua
{
    class CiA402
    {
    public:
        static bool isFault(uint16_t sw){return (sw & 0x006F) == 0x0008;}               
        static bool isSwitchOnDisabled(uint16_t sw){return (sw & 0x006F) == 0x0040;}
        static bool isReadyToSwitchOn(uint16_t sw){return (sw & 0x006F) == 0x0021;}
        static bool isSwitchedOn(uint16_t sw){return (sw & 0x006F) == 0x0023;}          
        static bool isOperationEnabled(uint16_t sw){return (sw & 0x006F) == 0x0027;}    
        static bool isTargetReached(uint16_t sw){return (sw & 0x0400);}

        static uint16_t getNextControlWord(uint16_t sw);
    };
}