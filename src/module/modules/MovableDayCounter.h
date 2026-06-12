#pragma once

#include <format>

#include "../HudModule.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Free-placement "days survived" counter. Derived from the world time the
// renderer is computing with (cached by the TimeChanger hook): day = ticks /
// 24000. Shows "—" until the hook has observed a time value.
class MovableDayCounter final : public HudModule {
public:
    MovableDayCounter()
        : HudModule("Movable Day Counter", "Freely repositionable in-game day counter",
                    Category::Misc, /*positioned*/ true, "", 8, 92) {}

protected:
    void writeHudBody(json::Object& o) const override {
        o.set("type", "panel").set("id", "moveday").set("label", "Day");
        if (const auto ticks = sdk::GameSDK::get().worldTime()) {
            o.set("value", std::to_string(*ticks / 24000));
        } else {
            o.set("value", "—");
        }
    }
};

} // namespace glacier
