#include "../HudModule.h"
#include "../ModuleRegistry.h"

namespace glacier {

// The simplest possible HudModule, and deliberately so: it proves the HUD
// placement/scale/background path works without depending on a single game
// signature. If the watermark draws and drags, every future HUD widget only has
// to fill in writeHudBody().
class Watermark final : public HudModule {
public:
    Watermark()
        : HudModule("Watermark", "Draws the Glacier logo on screen", 0, 0.01f, 0.01f) {
        addSetting(Setting{ "showfps", "Show FPS", true });
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        auto& r = ui::Renderer::get();

        const float size = 16.0f * scale;
        const std::string text = buildText();
        const float w = r.measureText(text, size, true);
        const float h = size * 1.4f;

        r.drawText(text, ui::Rect{ origin.x, origin.y, w + 4.0f, h },
                   ui::Color::rgba(0xFF4C9AFF), size, ui::TextAlign::Left, true);

        return ui::Rect{ origin.x, origin.y, w, h };
    }

private:
    std::string buildText() {
        std::string text = "Glacier";
        if (const auto* s = setting("showfps"); s && s->asBool()) {
            text += "  " + std::to_string(currentFps()) + " fps";
        }
        return text;
    }

    // Frame counter sampled over a one-second window. Cheap and good enough for
    // a HUD readout; a rolling average would be smoother but is Phase 4 polish.
    int currentFps() {
        ++m_frames;
        const ULONGLONG now = GetTickCount64();
        if (now - m_lastSample >= 1000) {
            m_fps = static_cast<int>(m_frames * 1000ULL / (now - m_lastSample));
            m_frames = 0;
            m_lastSample = now;
        }
        return m_fps;
    }

    ULONGLONG m_lastSample = GetTickCount64();
    unsigned  m_frames = 0;
    int       m_fps = 0;
};

GLACIER_MODULE(Watermark);

} // namespace glacier
