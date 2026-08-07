#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../util/FrameStats.h"

namespace glacier {

// The simplest possible HudModule, and deliberately so: it proves the HUD
// placement/scale/background path works without depending on a single game
// signature. If the watermark draws and drags, every future HUD widget only has
// to fill in writeHudBody().
class Watermark final : public HudModule {
public:
    Watermark()
        : HudModule("Watermark", "Draws the Glacier logo on screen", 0,
                    0.01f, 0.01f, 0xFF4C9AFF) {
        addSetting(Setting{ "showfps", "Show FPS", true });
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        auto& r = ui::Renderer::get();

        const float size = 16.0f * scale;
        const std::string text = buildText();
        const float w = r.measureText(text, size, true);
        const float h = size * 1.4f;

        r.drawText(text, ui::Rect{ origin.x, origin.y, w + 4.0f, h },
                   textColor(), size, ui::TextAlign::Left, true);

        return ui::Rect{ origin.x, origin.y, w, h };
    }

private:
    std::string buildText() {
        std::string text = "Glacier";
        if (const auto* s = setting("showfps"); s && s->asBool()) {
            // Shared source, so this and the FPS Counter module can never
            // disagree about the number.
            text += "  " + std::to_string(FrameStats::get().fps()) + " fps";
        }
        return text;
    }
};

GLACIER_MODULE(Watermark);

} // namespace glacier
