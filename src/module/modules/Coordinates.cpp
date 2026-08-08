#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace glacier {

// Player position readout.
//
// Depends only on Actor::position plus the ClientInstance capture, so it is the
// cheapest possible check that the SDK is actually reaching the player: if
// Coordinates shows numbers that track your movement, the whole
// signature → hook → ClientInstance → LocalPlayer chain is working.
class Coordinates final : public TextHudModule {
public:
    Coordinates()
        : TextHudModule("Coordinates", "Shows your position in the world",
                    Category::Player, 0,
                    0.01f, 0.13f, 0xFFFFFFFF) {
        addSetting(Setting{ "decimals", "Decimal places", 1, 0, 3 });
        addSetting(Setting{ "labels", "Show X/Y/Z labels", true });
        addSetting(Setting{ "nether", "Show Nether conversion", false });
    }

    void buildLines(std::vector<Line>& out) override {
        const auto pos = sdk::GameSDK::get().playerPosition();

        // "not in a world yet" and "offsets are wrong" look identical from here,
        // so say the neutral thing rather than implying a failure.
        if (!pos) {
            out.push_back(Line{ "XYZ --", false, textColor().withAlpha(0.5f) });
            return;
        }

        push(out, format(pos->x, pos->y, pos->z));

        if (settingBool("nether", false)) {
            // Overworld→Nether is an 8:1 horizontal ratio; Y is unchanged.
            pushSecondary(out, "Nether " + format(pos->x / 8.0f, pos->y, pos->z / 8.0f));
        }
    }

private:
    std::string format(float x, float y, float z) const {
        int decimals = 1;
        if (const auto* s = setting("decimals")) decimals = s->asInt();

        const bool labels = settingBool("labels", true);
        char buf[128];

        if (labels) {
            std::snprintf(buf, sizeof(buf), "X %.*f  Y %.*f  Z %.*f",
                          decimals, x, decimals, y, decimals, z);
        } else {
            std::snprintf(buf, sizeof(buf), "%.*f  %.*f  %.*f",
                          decimals, x, decimals, y, decimals, z);
        }
        return buf;
    }
};

GLACIER_MODULE(Coordinates);

} // namespace glacier
