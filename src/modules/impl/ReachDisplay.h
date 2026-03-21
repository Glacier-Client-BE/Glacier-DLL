#pragma once
#include "../ModuleBase.h"
#include <deque>

class ReachDisplay : public ModuleBase {
public:
    ReachDisplay();
    void onEnable()      override;
    void onDisable()     override;
    void onTick()        override;
    void onRenderImGui() override;
private:
    std::deque<float> m_samples;
    float             m_lastRec = -1.f;
};
