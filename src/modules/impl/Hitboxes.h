#pragma once
#include "../ModuleBase.h"

class Hitboxes : public ModuleBase {
public:
    Hitboxes();
    void onRender(ImDrawList* dl) override;
};
