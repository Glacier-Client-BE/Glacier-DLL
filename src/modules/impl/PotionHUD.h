#pragma once
#include "../ModuleBase.h"

class PotionHUD : public ModuleBase {
public:
    PotionHUD();
    void onRenderImGui() override;
};
