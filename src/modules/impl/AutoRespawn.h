#pragma once
#include "../ModuleBase.h"
#include <chrono>

class AutoRespawn : public ModuleBase {
public:
    AutoRespawn();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point m_deathTime = {};
    bool              m_waiting   = false;
};
