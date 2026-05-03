#pragma once
#include "../Module.h"

namespace Glacier::modules {

class Keystrokes final : public Module {
public:
    Keystrokes();
    void onRender(RenderHUDEvent& e);
private:
    FloatSetting& scale_;
    BoolSetting&  showMouse_;
    ColorSetting& accent_;
    FloatSetting& posX_;
    FloatSetting& posY_;
};

} // namespace Glacier::modules
