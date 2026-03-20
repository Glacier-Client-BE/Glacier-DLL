#pragma once
#include "../ModuleBase.h"
#include <string>

class TargetHUD : public ModuleBase {
public:
    TargetHUD();
    void onTick()        override;
    void onRenderImGui() override;
private:
    float       m_dispHP  = 1.f;
    float       m_fadeIn  = 0.f;
    std::string m_lastName;
};
