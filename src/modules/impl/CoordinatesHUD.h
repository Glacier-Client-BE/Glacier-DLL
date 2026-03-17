#pragma once
#include "../ModuleBase.h"

class CoordinatesHUD : public ModuleBase {
public:
    CoordinatesHUD();
    void onRenderImGui() override;
};
