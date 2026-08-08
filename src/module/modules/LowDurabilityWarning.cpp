#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <array>
#include <cstdio>
#include <string>

namespace glacier {

// Names the armor slot with the least durability left, once it drops below a
// threshold. Held-item durability isn't included: a tool you are actively
// using tells you it's about to break by other means (the game's own icon
// cracks), but armor gives no such warning until it's gone.
//
// Draws nothing at all above the threshold — TextHudModule skips an empty
// line list rather than leaving a box on screen, which is exactly the point:
// this widget should be invisible until it has something worth saying.
class LowDurabilityWarning final : public TextHudModule {
public:
    LowDurabilityWarning()
        : TextHudModule("Low Durability", "Warns when armor is close to breaking",
                    Category::Player, 0,
                    0.30f, 0.01f, 0xFFF87171) {
        addSetting(Setting{ "threshold", "Warn below (%)", 15.0f, 1.0f, 50.0f, 1.0f });
    }

    void buildLines(std::vector<Line>& out) override {
        auto& sdkRef = sdk::GameSDK::get();
        if (!sdkRef.armorSupported()) return;

        static constexpr std::array<const char*, 4> kNames{
            "Helmet", "Chestplate", "Leggings", "Boots"
        };

        const float threshold = settingFloat("threshold", 15.0f) / 100.0f;
        const auto armor = sdkRef.armor();

        for (std::size_t i = 0; i < armor.size(); ++i) {
            const auto& stack = armor[i];
            if (!stack.valid || stack.maxDurability <= 0) continue;

            const float frac = stack.durabilityFraction();
            if (frac > threshold) continue;

            char buf[48];
            std::snprintf(buf, sizeof(buf), "%s %d%%", kNames[i],
                          static_cast<int>(frac * 100.0f + 0.5f));
            out.push_back(Line{ buf, false, textColor() });
        }
    }
};

GLACIER_MODULE(LowDurabilityWarning);

} // namespace glacier
