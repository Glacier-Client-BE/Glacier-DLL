#pragma once

#include <format>
#include <Windows.h>

#include "../HudModule.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Info-only reach readout: the distance of your most recent landed hit, fed by
// the GameMode::attack hook. Holds the value for a configurable window, then
// clears so stale numbers don't linger between fights.
class ReachDisplay final : public HudModule {
public:
    ReachDisplay()
        : HudModule("Reach Display", "Show your attack reach distance (info only)",
                    Category::Combat, /*positioned*/ false, "top-right") {
        addSetting(Setting{ "hold", "Hold time (s)", 5, 1, 30 });
    }

protected:
    void writeHudBody(json::Object& o) const override {
        const auto attack = sdk::GameSDK::get().lastAttack();

        int holdS = 5;
        if (const auto* s = setting("hold")) holdS = s->asInt();

        const bool fresh = attack.valid &&
            (GetTickCount64() - attack.timeMs) < static_cast<unsigned long long>(holdS) * 1000ull;

        o.set("type", "text").set("id", "reach").set("anchor", anchor())
         .set("label", "Reach")
         .set("value", fresh ? std::format("{:.2f} m", attack.distance) : "—");
    }
};

} // namespace glacier
