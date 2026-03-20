#pragma once
#include "../ModuleBase.h"
#include <deque>
#include <chrono>

class CPSCounter : public ModuleBase {
public:
    CPSCounter();
    void onEnable()  override;
    void onRenderImGui() override;
private:
    using Clock = std::chrono::high_resolution_clock;
    using TP    = Clock::time_point;
    std::deque<TP> m_L, m_R;
    bool  m_pL = false, m_pR = false;
    int   m_peakL = 0, m_peakR = 0;
};
