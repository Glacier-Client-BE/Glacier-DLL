#pragma once
#include "../Module.h"
#include <deque>
#include <chrono>

namespace Glacier::modules {

class CPSCounter final : public Module {
public:
    CPSCounter();
    void onRender(RenderHUDEvent& e);
    void onKey(KeyEvent& k);
private:
    EnumSetting&  position_;
    ColorSetting& accent_;

    using Clock = std::chrono::steady_clock;
    std::deque<Clock::time_point> lmbHits_, rmbHits_;
};

} // namespace Glacier::modules
