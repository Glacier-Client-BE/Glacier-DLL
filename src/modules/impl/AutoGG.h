#pragma once
#include "../ModuleBase.h"
#include <string>

class AutoGG : public ModuleBase {
public:
    AutoGG();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    bool m_sentGG    = false;
    bool m_sentGL    = false;
    int  m_cooldown  = 0;
};
