#pragma once
#include "../ModuleBase.h"

class AutoSprint : public ModuleBase {
public:
    AutoSprint();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    bool m_sprinting = false;
};
