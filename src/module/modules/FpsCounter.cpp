#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../util/FrameStats.h"

#include <string>

namespace glacier {

// Frame-rate readout. Needs no game signatures at all — it counts Present
// calls — so it works on any Bedrock build the overlay itself works on.
class FpsCounter final : public HudModule {
public:
    FpsCounter()
        : HudModule("FPS Counter", "Shows the current frame rate",
                    Category::Misc, 0,
                    0.01f, 0.05f, 0xFFFFFFFF) {
        addSetting(Setting{ "label", "Show \"FPS\" label", true });
        addSetting(Setting{ "colorcode", "Color by performance", false });
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        auto& r = ui::Renderer::get();

        const int fps = FrameStats::get().fps();
        std::string text = std::to_string(fps);
        if (const auto* s = setting("label"); s && s->asBool()) {
            text += " FPS";
        }

        const float size = 16.0f * scale;
        const float w = r.measureText(text, size, true);
        const float h = size * 1.4f;

        r.drawText(text, ui::Rect{ origin.x, origin.y, w + 4.0f, h },
                   resolveColor(fps), size, ui::TextAlign::Left, true);

        return ui::Rect{ origin.x, origin.y, w, h };
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
