#pragma once
#include "../ModuleBase.h"

class FreeLook : public ModuleBase {
public:
    FreeLook();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    float m_savedYaw   = 0.f;
    float m_savedPitch = 0.f;
    bool  m_active     = false;
};
