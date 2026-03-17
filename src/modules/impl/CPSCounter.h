#pragma once
#include "../ModuleBase.h"
#include <chrono>
#include <deque>
class CPSCounter : public ModuleBase {
public:
    CPSCounter();
    void onEnable() override;
    void onRenderImGui() override;
private:
    using Clock = std::chrono::high_resolution_clock;
    std::deque<Clock::time_point> m_L, m_R;
    bool m_pL=false, m_pR=false;
};
