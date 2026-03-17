#pragma once
#include "../ModuleBase.h"

class TargetHUD : public ModuleBase {
public:
    TargetHUD();
    void onTick()        override;
    void onRenderImGui() override;
private:
    float m_displayedHealthRatio = 1.f;
};
