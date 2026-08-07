#pragma once

#include "Module.h"
#include "../ui/Renderer.h"

// Base for modules that draw a persistent on-screen widget.
//
// Position, anchor, and scale are baked in here rather than reimplemented by
// every widget: a HUD module supplies only its content via writeHudBody(), and
// gets placement, scaling, and a background for free. Positions are stored
// normalized (0..1 of the screen), so a widget stays where the user put it
// across resolution changes instead of drifting off-screen.
namespace glacier {

class HudModule : public Module {
public:
    HudModule(std::string name, std::string description, int keybind = 0,
              float defaultX = 0.02f, float defaultY = 0.02f)
        : Module(std::move(name), std::move(description), Category::Visual, keybind) {
        addSetting(Setting{ "hud.x", "X", defaultX, 0.0f, 1.0f, 0.001f });
        addSetting(Setting{ "hud.y", "Y", defaultY, 0.0f, 1.0f, 0.001f });
        addSetting(Setting{ "hud.scale", "Scale", 1.0f, 0.5f, 2.5f, 0.05f });
        addSetting(Setting{ "hud.background", "Background", true });
    }

    // Concrete widgets implement this. `origin` is the top-left the widget
    // should draw from; `scale` is already resolved from the setting. Return
    // the size actually drawn so the base can size the background behind it.
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

        // Two-pass: measure by drawing into a clipped zero-area region would be
        // wasteful, so instead the body reports its bounds and the background is
        // drawn on the *next* frame's cached size. One frame of lag on a
        // background box is invisible; measuring twice per frame is not free.
        if (m_lastBounds.w > 0.0f && settingBool("hud.background", true)) {
            const ui::Rect bg{
                origin.x - 4.0f * scale, origin.y - 2.0f * scale,
                m_lastBounds.w + 8.0f * scale, m_lastBounds.h + 4.0f * scale
            };
            r.fillRoundedRect(bg, 4.0f, ui::Color::rgba(0x80000000));
        }

        m_lastBounds = writeHudBody(origin, scale);
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

private:
    ui::Rect m_lastBounds{};
};

} // namespace glacier
