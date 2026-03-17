#pragma once
#include "../ModuleBase.h"
class KillCounter : public ModuleBase {
public:
    KillCounter();
    void onEnable() override;
    void onRenderImGui() override;
    void addKill(); void addDeath();
private:
    int m_kills=0, m_deaths=0, m_assists=0;
};
