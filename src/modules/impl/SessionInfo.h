#pragma once
#include "../ModuleBase.h"
#include <chrono>
#include <Psapi.h>

class SessionInfo : public ModuleBase {
public:
    SessionInfo();
    void onEnable()      override;
    void onRenderImGui() override;
private:
    std::chrono::high_resolution_clock::time_point m_start{};
    float m_fpsAccum = 0.f;
    int   m_fpsCount = 0;
    float m_avgFPS   = 0.f;
};
