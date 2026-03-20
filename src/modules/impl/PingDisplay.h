#pragma once
#include "../ModuleBase.h"
#include <deque>

class PingDisplay : public ModuleBase {
public:
    PingDisplay();
    void onEnable()      override;
    void onDisable()     override;
    void onRenderImGui() override;
private:
    std::deque<float> m_history;
    float             m_accumDt = 0.f;
};
