#pragma once
#include "../ModuleBase.h"
class Clock : public ModuleBase {
public:
    Clock();
    void onRenderImGui() override;
};
