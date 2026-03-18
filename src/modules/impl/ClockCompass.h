#pragma once
#include "../ModuleBase.h"

class ClockCompass : public ModuleBase {
public:
    ClockCompass();
    void onRenderImGui() override;
};
