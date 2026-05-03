#pragma once
#include "../Module.h"

namespace Glacier::modules {

// Client-side world-time override (visual only, server-side world-time is
// untouched). Hooks Level::getTimeOfDay once Addresses are wired.
class TimeChanger final : public Module {
public:
    TimeChanger();
private:
    EnumSetting&  preset_;   // Day / Sunset / Night / Sunrise / Custom
    IntSetting&   custom_;   // 0..24000
};

} // namespace Glacier::modules
