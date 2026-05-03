#pragma once
#include "../Module.h"

namespace Glacier::modules {

class FPSCounter final : public Module {
public:
    FPSCounter();
    void onRender(RenderHUDEvent& e);
private:
    EnumSetting&  position_;
    BoolSetting&  graph_;
    ColorSetting& accent_;
    float fps_  = 0.f;
    float hist_[120]{};
    int   head_ = 0;
};

} // namespace Glacier::modules
