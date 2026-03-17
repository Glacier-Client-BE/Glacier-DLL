#pragma once
#include "../ModuleBase.h"
#include <chrono>
#include <deque>
class ClickStatsDashboard : public ModuleBase {
public:
    ClickStatsDashboard();
    void onRenderImGui() override;
private:
    using Clock=std::chrono::high_resolution_clock;
    std::deque<Clock::time_point> m_clicks;
    float m_peakCPS=0.f; bool m_pLMB=false;
};
