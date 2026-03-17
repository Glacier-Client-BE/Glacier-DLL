#pragma once
#include "../ModuleBase.h"

// Draws a health bar above every player in the world, projected in 3-D.
class HealthIndicator : public ModuleBase {
public:
    HealthIndicator();
    void onRender(ImDrawList* dl) override;
};
