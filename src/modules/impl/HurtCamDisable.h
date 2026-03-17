#pragma once
#include "../ModuleBase.h"

class HurtCamDisable : public ModuleBase {
public:
    HurtCamDisable();
    void onTick() override;
};
