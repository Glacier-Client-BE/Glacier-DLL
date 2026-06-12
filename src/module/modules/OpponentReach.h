#pragma once

#include <format>
#include <Windows.h>

#include "../HudModule.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Info-only readout of the distance an opponent hit you from. Fed by the same
// GameMode::attack hook as Reach Display, filtered to events where the local
// player is the *target*. Note: most servers don't simulate other players'
// attacks client-side, so this populates mainly in LAN / locally-simulated
// combat; it shows "—" when no incoming hit has been observed.
class OpponentReach final : public HudModule {
public:
    OpponentReach()
        : HudModule("Opponent Reach", "Show the distance at which an opponent hit you",
                    Category::Combat, /*positioned*/ false, "top-right") {
        addSetting(Setting{ "hold", "Hold time (s)", 5, 1, 30 });
    }

protected:
    void writeHudBody(json::Object& o) const override {
        const auto hit = sdk::GameSDK::get().lastIncomingHit();

        int holdS = 5;
        if (const auto* s = setting("hold")) holdS = s->asInt();

        const bool fresh = hit.valid &&
            (GetTickCount64() - hit.timeMs) < static_cast<unsigned long long>(holdS) * 1000ull;

        o.set("type", "text").set("id", "oppreach").set("anchor", anchor())
         .set("label", "Opp. Reach")
         .set("value", fresh ? std::format("{:.2f} m", hit.distance) : "—");
    }
};

} // namespace glacier
