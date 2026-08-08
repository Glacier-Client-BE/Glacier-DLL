#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../util/FrameStats.h"

#include <string>

namespace glacier {

// Frame-rate readout. Needs no game signatures at all — it counts Present
// calls — so it works on any Bedrock build the overlay itself works on.
class FpsCounter final : public TextHudModule {
public:
    FpsCounter()
        : TextHudModule("FPS Counter", "Shows the current frame rate",
                    Category::Misc, 0,
                    0.01f, 0.05f, 0xFFFFFFFF) {
        setIcon(0xF625);   // fa-gauge-high
        addSetting(Setting{ "label", "Show \"FPS\" label", true });
        addSetting(Setting{ "colorcode", "Color by performance", false });
    }

    void buildLines(std::vector<Line>& out) override {
        const int fps = FrameStats::get().fps();
        std::string text = std::to_string(fps);
        if (const auto* s = setting("label"); s && s->asBool()) {
            text += " FPS";
        }
        out.push_back(Line{ std::move(text), false, resolveColor(fps) });
    }

private:
    // Optional traffic-light coloring. Thresholds are deliberately generous —
    // this is a glanceable indicator, not a benchmark.
    ui::Color resolveColor(int fps) const {
        const auto* s = setting("colorcode");
        if (!s || !s->asBool()) return textColor();

        if (fps >= 120) return ui::Color::rgba(0xFF4ADE80);
        if (fps >= 60)  return ui::Color::rgba(0xFFFACC15);
        return ui::Color::rgba(0xFFF87171);
    }
};

GLACIER_MODULE(FpsCounter);

} // namespace glacier
