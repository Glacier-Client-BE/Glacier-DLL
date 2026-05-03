#pragma once
#include "../Module.h"

namespace Glacier::modules {

class ArmorHUD final : public Module {
public:
    ArmorHUD();
    void onRender(RenderHUDEvent& e);
private:
    EnumSetting&  position_;
    BoolSetting&  vertical_;
    ColorSetting& accent_;
};

} // namespace Glacier::modules
