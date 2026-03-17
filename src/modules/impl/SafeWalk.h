#pragma once
#include "../ModuleBase.h"

class SafeWalk : public ModuleBase {
public:
    SafeWalk();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    bool m_wasSneaking = false;
};
