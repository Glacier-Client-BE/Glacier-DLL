#pragma once
#include "../ModuleBase.h"

class FullBright : public ModuleBase {
public:
    FullBright();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
};
