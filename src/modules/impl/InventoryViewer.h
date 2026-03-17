#pragma once
#include "../ModuleBase.h"
class InventoryViewer : public ModuleBase {
public:
    InventoryViewer();
    void onRenderImGui() override;
};
