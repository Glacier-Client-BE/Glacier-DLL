#pragma once
#include "../Module.h"

namespace Glacier::modules {

class Coordinates final : public Module {
public:
    Coordinates();
    void onRender(RenderHUDEvent& e);
private:
    EnumSetting&  position_;
    BoolSetting&  showDirection_;
    BoolSetting&  showDimension_;
    ColorSetting& accent_;
};

} // namespace Glacier::modules
