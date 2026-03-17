#pragma once
#include "../ModuleBase.h"
class Crosshair : public ModuleBase {
public:
    Crosshair();
    void onRender(ImDrawList* dl) override;
};
