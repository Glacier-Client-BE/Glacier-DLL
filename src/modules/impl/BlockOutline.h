#pragma once
#include "../ModuleBase.h"

class BlockOutline : public ModuleBase {
public:
    BlockOutline();
    void onEnable()  override;
    void onDisable() override;
};
