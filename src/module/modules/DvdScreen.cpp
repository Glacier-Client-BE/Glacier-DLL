#include "../Module.h"
#include "../ModuleRegistry.h"
#include "../../ui/Renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace glacier {

// The bouncing-logo screensaver gag, as a HUD easter egg — Flarial ships the
// same idea with real logo textures; this draws the shape with the
// renderer's own primitives instead of adding an image-loading pipeline for
// one novelty widget.
//
// A plain Module rather than a HudModule: HudModule's position is a fixed
// point the user drags, and this widget owns a freely moving position of its
// own. Bolting a "bouncing" mode onto the shared drag system for one widget
// would be more machinery than the widget itself.
class DvdScreen final : public Module {
public:
    DvdScreen()
        : Module("DVD Screen", "The bouncing screensaver logo", Category::Visual, 0) {
        setIcon(0xF390);   // fa-display
        addSetting(Setting{ "width", "Width", 140.0f, 60.0f, 400.0f, 5.0f });
        addSetting(Setting{ "height", "Height", 70.0f, 30.0f, 200.0f, 5.0f });
        addSetting(Setting{ "speed", "Speed (px/s)", 220.0f, 40.0f, 800.0f, 10.0f });
    }

    void onEnable() override {
        m_lastTick = GetTickCount64();
    }

    void onRender() override {
        auto& r = ui::Renderer::get();
        if (!r.ready()) return;

        const float w = settingFloat("width", 140.0f);
        const float h = settingFloat("height", 70.0f);
        const float speed = settingFloat("speed", 220.0f);

        const ULONGLONG now = GetTickCount64();
        // Clamped rather than raw: the first frame after (re-)enabling, or
        // after the game hitches, would otherwise teleport the logo across
        // the screen in one step.
        const float dt = std::min(static_cast<float>(now - m_lastTick) / 1000.0f, 0.1f);
        m_lastTick = now;

        m_x += speed * dt * m_dirX;
        m_y += speed * dt * m_dirY;

        const float maxX = std::max(0.0f, r.width() - w);
        const float maxY = std::max(0.0f, r.height() - h);
        bool bounced = false;
        if (m_x <= 0.0f) { m_x = 0.0f; m_dirX = 1.0f; bounced = true; }
        if (m_x >= maxX) { m_x = maxX; m_dirX = -1.0f; bounced = true; }
        if (m_y <= 0.0f) { m_y = 0.0f; m_dirY = 1.0f; bounced = true; }
        if (m_y >= maxY) { m_y = maxY; m_dirY = -1.0f; bounced = true; }
        if (bounced) m_colorIndex = (m_colorIndex + 1) % kPalette.size();

        const ui::Rect box{ m_x, m_y, w, h };
        r.fillRoundedRect(box, h * 0.18f, kPalette[m_colorIndex]);
        r.drawText("DVD", box, ui::Color::rgba(0xFFFFFFFF), h * 0.42f,
                   ui::TextAlign::Center, ui::FontWeight::SemiBold, true);
    }

private:
    float settingFloat(std::string_view id, float fallback) const {
        const auto* s = setting(id);
        return s ? s->asFloat() : fallback;
    }

    static constexpr std::array<ui::Color, 6> kPalette{
        ui::Color::rgba(0xFFEF4444), ui::Color::rgba(0xFFF59E0B),
        ui::Color::rgba(0xFF4ADE80), ui::Color::rgba(0xFF38BDF8),
        ui::Color::rgba(0xFFA78BFA), ui::Color::rgba(0xFFEC4899),
    };

    float m_x = 40.0f, m_y = 60.0f;
    float m_dirX = 1.0f, m_dirY = 1.0f;
    std::size_t m_colorIndex = 0;
    ULONGLONG m_lastTick = 0;
};

GLACIER_MODULE(DvdScreen);

} // namespace glacier
