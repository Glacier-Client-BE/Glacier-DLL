#pragma once
#include "../ModuleBase.h"
#include <chrono>
class ComboCounter : public ModuleBase {
public:
    ComboCounter();
    void onEnable() override;
    void onRenderImGui() override;
    void registerHit(); void resetCombo();
private:
    using Clock = std::chrono::high_resolution_clock;
    int m_combo=0, m_maxCombo=0;
    Clock::time_point m_lastHit;
    bool m_pLMB=false;
};
