#pragma once
#include "../ModuleBase.h"
#include <chrono>
#include <Windows.h>

class HitColor : public ModuleBase {
public:
    HitColor();
    void onEnable()  override;
    void onDisable() override;
    void onRender(ImDrawList* dl) override;
private:
    using Clock = std::chrono::high_resolution_clock;
    bool         m_hitFlash    = false;
    bool         m_dmgFlash    = false;
    bool         m_prevLMB     = false;
    bool         m_wasDamaged  = false;
    Clock::time_point m_lastHitTime{};
    Clock::time_point m_lastDmgTime{};
};
