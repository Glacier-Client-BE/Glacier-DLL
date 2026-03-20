#pragma once
#include "../ModuleBase.h"
#include <array>
#include <string>

struct ArmorSlot {
    const char* icon;
    const char* label;
    int         dur, maxDur;
    bool        hasItem;
    std::string itemName;
};

class ArmorHUD : public ModuleBase {
public:
    ArmorHUD();
    void onRenderImGui() override;
private:
    std::array<ArmorSlot, 5> getSlots() const;
};
