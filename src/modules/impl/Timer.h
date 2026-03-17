#pragma once
#include "../ModuleBase.h"

// Speeds up or slows down the game tick rate client-side by scaling deltaTime.
class Timer : public ModuleBase {
public:
    Timer();
    void onEnable()  override;
    void onDisable() override;
    void onTick()    override;
private:
    float m_tickAccum = 0.f;
};
