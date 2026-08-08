#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <cstdio>
#include <string>

namespace glacier {

// Distance of your most recent melee hit.
//
// Purely observational: the underlying hook records the distance and calls the
// game's attack through untouched. Glacier does not extend, modify, or trigger
// attacks — this reports what happened, it does not change what can happen.
// (See the scope boundary in README.)
class ReachDisplay final : public TextHudModule {
public:
    ReachDisplay()
        : TextHudModule("Reach Display", "Shows the distance of your last hit",
                    Category::Combat, 0,
                    0.01f, 0.25f, 0xFFFFFFFF) {
        addSetting(Setting{ "hold", "Seconds to display", 3.0f, 0.5f, 15.0f, 0.5f });
        addSetting(Setting{ "fade", "Fade out", true });
    }

    void buildLines(std::vector<Line>& out) override {
        std::uint64_t ageMs = 0;
        const auto distance = sdk::GameSDK::get().lastAttackDistance(&ageMs);

        const float holdMs = settingFloat("hold", 3.0f) * 1000.0f;

        std::string text;
        float alpha = 1.0f;

        if (!distance || static_cast<float>(ageMs) > holdMs) {
            text = "Reach --";
            alpha = 0.45f;
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Reach %.2f", *distance);
            text = buf;

            // Fade across the last third of the hold window, so the number
            // doesn't simply vanish mid-glance.
            if (settingBool("fade", true)) {
                const float fadeStart = holdMs * 0.66f;
                if (static_cast<float>(ageMs) > fadeStart && holdMs > fadeStart) {
                    alpha = 1.0f - (static_cast<float>(ageMs) - fadeStart) / (holdMs - fadeStart);
                    alpha = alpha < 0.0f ? 0.0f : alpha;
                }
            }
        }

        out.push_back(Line{ std::move(text), false, textColor().withAlpha(alpha) });
    }
};

GLACIER_MODULE(ReachDisplay);

} // namespace glacier
