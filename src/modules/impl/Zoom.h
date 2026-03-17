#pragma once
#include "../ModuleBase.h"

class Zoom : public ModuleBase {
public:
    Zoom();
    void onDisable() override;
    void onTick()    override;
private:
    float m_currentFovMult = 1.f;
};
