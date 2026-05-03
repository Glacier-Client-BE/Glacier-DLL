#pragma once
#include "../Module.h"

namespace Glacier::modules {

class Crosshair final : public Module {
public:
    Crosshair();
    void onRender(RenderHUDEvent& e);
private:
    EnumSetting&  style_;
    FloatSetting& size_;
    FloatSetting& thickness_;
    FloatSetting& gap_;
    BoolSetting&  outline_;
    ColorSetting& color_;
};

} // namespace Glacier::modules
