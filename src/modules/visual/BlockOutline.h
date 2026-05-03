#pragma once
#include "../Module.h"

namespace Glacier::modules {

class BlockOutline final : public Module {
public:
    BlockOutline();
    void onRender(RenderHUDEvent& e);
private:
    EnumSetting&  style_;        // Outline / Filled / Both
    FloatSetting& thickness_;
    ColorSetting& color_;
};

} // namespace Glacier::modules
