#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <string>

namespace glacier {

// Server round-trip time, sourced from the game's own ping query.
//
// The value is whatever the game last asked RakNet for — Glacier never calls
// into RakNet itself, since doing so off the network thread isn't safe. It
// follows that in single-player, or on the main menu, nothing queries ping and
// the reading correctly goes stale rather than freezing on an old number.
class PingDisplay final : public HudModule {
public:
    PingDisplay()
        : HudModule("Ping", "Shows your connection latency",
                    Category::Misc, 0,
                    0.01f, 0.17f, 0xFFFFFFFF) {
        addSetting(Setting{ "label", "Show \"ms\" unit", true });
        addSetting(Setting{ "colorcode", "Color by latency", true });
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        auto& r = ui::Renderer::get();

        const int ms = sdk::GameSDK::get().ping();
        const bool known = ms >= 0;

        std::string text = known ? std::to_string(ms) : "--";
        if (settingBool("label", true)) text += " ms";

        const float size = 15.0f * scale;
        const float w = r.measureText(text, size, true);
        const float h = size * 1.4f;

        r.drawText(text, ui::Rect{ origin.x, origin.y, w + 4.0f, h },
                   known ? resolveColor(ms) : textColor().withAlpha(0.45f),
                   size, ui::TextAlign::Left, true);

        return ui::Rect{ origin.x, origin.y, w, h };
    }

private:
    ui::Color resolveColor(int ms) const {
        if (!settingBool("colorcode", true)) return textColor();
        if (ms < 60)  return ui::Color::rgba(0xFF4ADE80);
        if (ms < 150) return ui::Color::rgba(0xFFFACC15);
        return ui::Color::rgba(0xFFF87171);
    }
};

GLACIER_MODULE(PingDisplay);

} // namespace glacier
