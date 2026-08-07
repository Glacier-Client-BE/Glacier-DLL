#pragma once

#include "Module.h"
#include "../ui/HudEditor.h"
#include "../ui/Renderer.h"

// Base for modules that draw a persistent on-screen widget.
//
// Position, anchor, scale, colors, and drag-to-move are baked in here rather
// than reimplemented by every widget: a HUD module supplies only its content
// via writeHudBody(), and gets placement, scaling, a background, a text color,
// and repositioning for free. Positions are stored normalized (0..1 of the
// screen) so a widget stays where the user put it across resolution changes
// instead of drifting off-screen.
namespace glacier {

class HudModule : public Module {
public:
    HudModule(std::string name, std::string description, int keybind = 0,
              float defaultX = 0.02f, float defaultY = 0.02f,
              std::uint32_t defaultColor = 0xFFFFFFFF)
        : Module(std::move(name), std::move(description), Category::Visual, keybind) {
        addSetting(Setting{ "hud.x", "X", defaultX, 0.0f, 1.0f, 0.001f });
        addSetting(Setting{ "hud.y", "Y", defaultY, 0.0f, 1.0f, 0.001f });
        addSetting(Setting{ "hud.scale", "Scale", 1.0f, 0.5f, 3.0f, 0.05f });
        addSetting(Setting{ "hud.color", "Color", Setting::ColorTag{}, defaultColor });
        addSetting(Setting{ "hud.background", "Background", true });
        addSetting(Setting{ "hud.bgcolor", "Background color",
                            Setting::ColorTag{}, 0x99000000 });
    }

    // Concrete widgets implement this. `origin` is the top-left to draw from and
    // `scale` is already resolved. Return the size actually drawn so the base
    // can size the background and the drag target behind it.
    virtual ui::Rect writeHudBody(const ui::Rect& origin, float scale) = 0;

    void onRender() final {
        auto& r = ui::Renderer::get();
        if (!r.ready()) return;

        const float scale = settingFloat("hud.scale", 1.0f);
        const ui::Rect origin{
            settingFloat("hud.x", 0.0f) * r.width(),
            settingFloat("hud.y", 0.0f) * r.height(),
            0, 0
        };

        // The background is sized from the previous frame's measured bounds.
        // Measuring twice per frame (once to size, once to draw) would double
        // the text-layout cost every frame; one frame of lag on a background
        // box is invisible, and only shows on the very first frame a widget
        // appears or immediately after its content changes width.
        if (m_lastBounds.w > 0.0f && settingBool("hud.background", true)) {
            const ui::Rect bg{
                origin.x - 4.0f * scale, origin.y - 2.0f * scale,
                m_lastBounds.w + 8.0f * scale, m_lastBounds.h + 4.0f * scale
            };
            r.fillRoundedRect(bg, 4.0f, ui::Color::rgba(settingColor("hud.bgcolor", 0x99000000)));
        }

        m_lastBounds = writeHudBody(origin, scale);

        // Drag-to-move, only while the menu is open.
        if (ui::HudEditor::get().active()) {
            float nx = settingFloat("hud.x", 0.0f);
            float ny = settingFloat("hud.y", 0.0f);
            if (ui::HudEditor::get().update(name(), m_lastBounds, nx, ny)) {
                if (auto* sx = setting("hud.x")) sx->set(nx);
                if (auto* sy = setting("hud.y")) sy->set(ny);
            }
        }
    }

protected:
    float settingFloat(std::string_view id, float fallback) const {
        const auto* s = setting(id);
        return s ? s->asFloat() : fallback;
    }
    bool settingBool(std::string_view id, bool fallback) const {
        const auto* s = setting(id);
        return s ? s->asBool() : fallback;
    }
    std::uint32_t settingColor(std::string_view id, std::uint32_t fallback) const {
        const auto* s = setting(id);
        return s ? s->asColor() : fallback;
    }

    // The widget's own text color, resolved from the shared "hud.color" setting.
    ui::Color textColor() const {
        return ui::Color::rgba(settingColor("hud.color", 0xFFFFFFFF));
    }

private:
    ui::Rect m_lastBounds{};
};

} // namespace glacier
