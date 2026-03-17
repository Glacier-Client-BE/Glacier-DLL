#pragma once
#include "../ModuleBase.h"
class ReachDisplay : public ModuleBase {
public:
    ReachDisplay();
    void onRenderImGui() override;
};
