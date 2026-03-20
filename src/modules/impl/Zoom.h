#pragma once
#include "../ModuleBase.h"
#include <Windows.h>

class Zoom : public ModuleBase {
public:
    Zoom();
    void onDisable() override;
    void onTick()    override;
private:
    float m_currentFovMult = 1.f;
    bool  m_wasHeld        = false;
};
