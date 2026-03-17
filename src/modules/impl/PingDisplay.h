#pragma once
#include "../ModuleBase.h"
class PingDisplay : public ModuleBase {
public:
    PingDisplay();
    void onRenderImGui() override;
};
