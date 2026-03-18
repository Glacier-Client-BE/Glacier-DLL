#pragma once
#include "../ModuleBase.h"

class EnvironmentChanger : public ModuleBase {
public:
    EnvironmentChanger();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    float m_savedGamma = 1.0f;
};
