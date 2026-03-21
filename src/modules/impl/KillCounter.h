#pragma once
#include "../ModuleBase.h"
#include <chrono>

class KillCounter : public ModuleBase {
public:
    KillCounter();
    void onEnable()      override;
    void onTick()        override;
    void onRenderImGui() override;
    void addKill();
    void addDeath();
private:
    int   m_kills      = 0;
    int   m_deaths     = 0;
    int   m_assists    = 0;
    int   m_streak     = 0;
    int   m_maxStreak  = 0;
    std::chrono::high_resolution_clock::time_point m_lastKill{};
};
