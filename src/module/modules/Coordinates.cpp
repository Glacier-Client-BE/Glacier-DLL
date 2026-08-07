#include "../HudModule.h"
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
class Coordinates final : public HudModule {
public:
    Coordinates()
        : HudModule("Coordinates", "Shows your position in the world",
                    Category::Player, 0,
                    0.01f, 0.13f, 0xFFFFFFFF) {
        addSetting(Setting{ "decimals", "Decimal places", 1, 0, 3 });
        addSetting(Setting{ "labels", "Show X/Y/Z labels", true });
        addSetting(Setting{ "nether", "Show Nether conversion", false });
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        auto& r = ui::Renderer::get();

        const auto pos = sdk::GameSDK::get().playerPosition();
        const float size = 15.0f * scale;
        const float lineH = size * 1.35f;

        // "not in a world yet" and "offsets are wrong" look identical from here,
        // so say the neutral thing rather than implying a failure.
        if (!pos) {
            const std::string text = "XYZ --";
            const float w = r.measureText(text, size, true);
            r.drawText(text, ui::Rect{ origin.x, origin.y, w + 4.0f, lineH },
                       textColor().withAlpha(0.5f), size, ui::TextAlign::Left, true);
            return ui::Rect{ origin.x, origin.y, w, lineH };
        }

        const std::string main = format(pos->x, pos->y, pos->z);
        float width = r.measureText(main, size, true);
        r.drawText(main, ui::Rect{ origin.x, origin.y, width + 4.0f, lineH },
                   textColor(), size, ui::TextAlign::Left, true);

        float height = lineH;

        if (settingBool("nether", false)) {
            // Overworld→Nether is an 8:1 horizontal ratio; Y is unchanged.
            const std::string nether =
                "Nether " + format(pos->x / 8.0f, pos->y, pos->z / 8.0f);
            const float nw = r.measureText(nether, size * 0.85f, false);
            r.drawText(nether,
                       ui::Rect{ origin.x, origin.y + lineH, nw + 4.0f, lineH },
                       textColor().withAlpha(0.65f), size * 0.85f, ui::TextAlign::Left);
            width = (nw > width) ? nw : width;
            height += lineH;
        }

        return ui::Rect{ origin.x, origin.y, width, height };
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
