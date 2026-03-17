#pragma once
#include "../ModuleBase.h"
#include <array>
#include <string>
struct ArmorSlot { std::string name; int dur, maxDur; };
class ArmorHUD : public ModuleBase {
public:
    ArmorHUD();
    void onRenderImGui() override;
private:
    std::array<ArmorSlot,4> getSlots() const;
    static ImU32 durColor(int d, int m);
};
