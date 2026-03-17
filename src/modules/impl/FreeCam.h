#pragma once
#include "../ModuleBase.h"

class FreeCam : public ModuleBase {
public:
    FreeCam();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
};
