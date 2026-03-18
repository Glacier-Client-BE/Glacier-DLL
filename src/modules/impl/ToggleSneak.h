#pragma once
#include "../ModuleBase.h"

class ToggleSneak : public ModuleBase {
public:
    ToggleSneak();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    bool m_sneakToggled = false;
};
