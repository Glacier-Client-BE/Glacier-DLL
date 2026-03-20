#pragma once
#include "../ModuleBase.h"
#include <Windows.h>

class Crosshair : public ModuleBase {
public:
    Crosshair();
    void onEnable()  override;
    void onDisable() override;
    void onRender(ImDrawList* dl) override;
private:
    bool  m_prevLMB  = false;
    float m_atkFlash = 0.f;   // 1→0 over atkFade ms
};
