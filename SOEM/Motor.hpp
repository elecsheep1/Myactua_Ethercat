#pragma once
#include "myactua/CiA402.hpp"
#include "myactua/EthercatAdapter.hpp"
#include <memory>

namespace myactua{

class MYACTUA
{
public:
    MYACTUA(std::shared_ptr<EthercatAdapter> adapter, int slave_index);

    bool connect(const char* ifname);
    void setMode(ControlMode mode);

    void process(double value);

    void updateRx();
    void updateCiA402();
    void updateMode();
    void updateTarget(double value);


private:
    std::shared_ptr<EthercatAdapter> adapter;
    int slave_index_;
    TxPDO tx_{};
    RxPDO rx_{};
    ControlMode current_mode_ = ControlMode::NONE;
    bool mode_confirmed_ = false;
};    

}
