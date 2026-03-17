#pragma once
#include "../ModuleBase.h"
#include <chrono>
#include <Psapi.h>
class SessionInfo : public ModuleBase {
public:
    SessionInfo();
    void onEnable() override;
    void onRenderImGui() override;
private:
    std::chrono::high_resolution_clock::time_point m_start;
};
