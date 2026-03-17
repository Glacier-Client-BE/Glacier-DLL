#pragma once
#include "../ModuleBase.h"
class Keystrokes : public ModuleBase {
public:
    Keystrokes();
    void onRenderImGui() override;
private:
    void drawKey(ImDrawList* dl, const char* label, float x, float y, float w, float h, bool pressed) const;
};
