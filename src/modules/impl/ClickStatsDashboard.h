#pragma once
#include "../ModuleBase.h"
#include <chrono>
#include <deque>
#include <numeric>

class ClickStatsDashboard : public ModuleBase {
public:
    ClickStatsDashboard();
    void onEnable()      override;
    void onRenderImGui() override;
private:
    using Clock = std::chrono::high_resolution_clock;
    std::deque<Clock::time_point> m_clicks;
    std::deque<float>             m_cpsHistory;
    float m_peakCPS    = 0.f;
    int   m_totalClicks = 0;
    bool  m_pLMB       = false;
    float m_accumDt    = 0.f;
};
