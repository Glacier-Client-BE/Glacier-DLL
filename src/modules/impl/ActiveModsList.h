#pragma once
#include "../ModuleBase.h"
class ActiveModsList : public ModuleBase {
public:
    ActiveModsList();
    void onRenderImGui() override;
};
