#pragma once
#include "../Module.h"
#include <chrono>

namespace Glacier::modules {

// Sends a configured message on an interval. Useful as an advert / channel
// keeper. Skips while the chat screen is open.
class AutoText final : public Module {
public:
    AutoText();
    void onTick(TickEvent& e);
private:
    TextSetting&  message_;
    IntSetting&   intervalSec_;
    std::chrono::steady_clock::time_point next_{};
};

} // namespace Glacier::modules
