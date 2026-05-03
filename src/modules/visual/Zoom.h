#pragma once
#include "../Module.h"

namespace Glacier::modules {

class Zoom final : public Module {
public:
    Zoom();
    void onKey(KeyEvent& k);
    void onRender(RenderHUDEvent& e);
private:
    FloatSetting& factor_;
    BoolSetting&  smooth_;
    KeybindSetting& holdKey_;
    bool  holding_ = false;
    float scroll_  = 0.f;
};

} // namespace Glacier::modules
