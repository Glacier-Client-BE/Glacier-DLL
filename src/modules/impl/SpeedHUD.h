#pragma once
#include "../ModuleBase.h"
#include <chrono>
#include <deque>

class SpeedHUD : public ModuleBase {
public:
    SpeedHUD();
    void onEnable()      override;
    void onRenderImGui() override;
private:
    using Clock = std::chrono::high_resolution_clock;
    std::deque<float>  m_history;
    float              m_lastX = 128.f, m_lastZ = -256.f;
    Clock::time_point  m_lastTime;
    float              m_bps = 0.f, m_maxBps = 0.f;
};
