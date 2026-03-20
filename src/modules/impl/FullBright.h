#pragma once
#include "../ModuleBase.h"
#include <cmath>

class FullBright : public ModuleBase {
public:
    FullBright();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    bool  m_restoring     = false;
    float m_restoreTarget = 1.0f;
};
