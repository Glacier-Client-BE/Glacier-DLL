#pragma once
#include "../Module.h"

namespace Glacier::modules {

class Watermark final : public Module {
public:
    Watermark();
    void onRender(RenderHUDEvent& e);
private:
    BoolSetting&  showVersion_;
    BoolSetting&  showFps_;
    EnumSetting&  position_;
    ColorSetting& accent_;
};

} // namespace Glacier::modules
