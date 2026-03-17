#pragma once
#include "../ModuleBase.h"
#include <chrono>
#include <deque>

class FPSCounter : public ModuleBase {
public:
    FPSCounter();
    void onEnable() override;
    void onDisable() override;
    void onRender(ImDrawList* dl) override;
private:
    using Clock = std::chrono::high_resolution_clock;
    float m_fps = 0.f;
    Clock::time_point m_last;
    std::deque<float> m_samples;
};
