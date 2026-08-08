#include "../TextHudModule.h"
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
class PingDisplay final : public TextHudModule {
public:
    PingDisplay()
        : TextHudModule("Ping", "Shows your connection latency",
                    Category::Misc, 0,
                    0.01f, 0.17f, 0xFFFFFFFF) {
        setIcon(0xF012);   // fa-signal
        addSetting(Setting{ "label", "Show \"ms\" unit", true });
        addSetting(Setting{ "colorcode", "Color by latency", true });
    }

    void buildLines(std::vector<Line>& out) override {
        const int ms = sdk::GameSDK::get().ping();
        const bool known = ms >= 0;

        std::string text = known ? std::to_string(ms) : "--";
        if (settingBool("label", true)) text += " ms";

        out.push_back(Line{ std::move(text), false,
                            known ? resolveColor(ms) : textColor().withAlpha(0.45f) });
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
