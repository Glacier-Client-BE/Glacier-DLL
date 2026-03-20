#pragma once
#include "../ModuleBase.h"
#include <chrono>
class ComboCounter : public ModuleBase {
public:
    ComboCounter();
    void onEnable() override; void onTick() override; void onRenderImGui() override;
private:
    using Clock = std::chrono::high_resolution_clock;
    int m_combo=0, m_max=0; bool m_pLMB=false, m_dead=false; float m_alpha=1.f;
    Clock::time_point m_lastHit{};
};
