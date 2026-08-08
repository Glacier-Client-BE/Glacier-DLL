#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <algorithm>
#include <cstdio>

namespace glacier {

// A durability bar for whatever is in your hand, after Flarial's
// DurabilityWarning.
//
// Low Durability already warns about *armor*, and armor is the case that needs
// warning about because you cannot see it. The held item is the opposite: you
// look at it constantly and still have no idea how close the pickaxe is to
// breaking, because vanilla only shows the bar once it is already low.
//
// Drawn rather than written as text, and drawn as a HudModule rather than a
// TextHudModule, because a fraction is exactly the thing a bar reads faster
// than a number — the percentage is there as a secondary label, not the point.
class ToolDurability final : public HudModule {
public:
    ToolDurability()
        : HudModule("Tool Durability", "Shows the held item's remaining durability",
                    Category::Player, 0,
                    0.42f, 0.86f, 0xFFFFFFFF) {
        setIcon(0xF0E4);   // fa-gauge
        addSetting(Setting{ "width", "Bar width", 92.0f, 40.0f, 240.0f, 2.0f });
        addSetting(Setting{ "height", "Bar height", 7.0f, 3.0f, 20.0f, 0.5f });
        addSetting(Setting{ "percent", "Show percentage", true });
        addSetting(Setting{ "gradient", "Colour by remaining", true });
        addSetting(Setting{ "hideundamaged", "Hide for undamaged items", true });
    }

    // Computed from settings alone, so the background is sized from this
    // frame's content rather than last frame's — see HudModule::measureHudBody.
    ui::Rect measureHudBody(float scale) override {
        if (!visible()) return ui::Rect{};

        const float barW = settingFloat("width", 92.0f) * scale;
        const float barH = settingFloat("height", 7.0f) * scale;
        if (!settingBool("percent", true)) return ui::Rect{ 0, 0, barW, barH };

        // The label sits on its own line under the bar, on the shared line
        // rhythm rather than a hand-picked gap.
        return ui::Rect{ 0, 0, barW, barH + lineHeight(scale) };
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        if (!visible()) return ui::Rect{};

        auto& r = ui::Renderer::get();

        const float fraction = std::clamp(heldFraction(), 0.0f, 1.0f);
        const float barW = settingFloat("width", 92.0f) * scale;
        const float barH = settingFloat("height", 7.0f) * scale;
        const ui::Rect bar{ origin.x, origin.y, barW, barH };

        // Track is the widget's own colour at low alpha rather than a fixed
        // grey, so recolouring the widget recolours the whole thing.
        r.fillRoundedRect(bar, barH * 0.5f, textColor().withAlpha(0.18f));
        if (fraction > 0.0f) {
            r.fillRoundedRect(ui::Rect{ bar.x, bar.y, barW * fraction, barH },
                              barH * 0.5f, barColor(fraction));
        }

        if (!settingBool("percent", true)) {
            return ui::Rect{ origin.x, origin.y, barW, barH };
        }

        char label[16];
        std::snprintf(label, sizeof(label), "%d%%", static_cast<int>(fraction * 100.0f + 0.5f));
        drawLine(ui::Rect{ origin.x, origin.y + barH, 0, 0 }, label, scale, true);

        return ui::Rect{ origin.x, origin.y, barW, barH + lineHeight(scale) };
    }

private:
    // Empty hand, or an item with no durability at all, is nothing to report.
    // Note the deliberate asymmetry with "hide undamaged": a full-durability
    // tool IS meaningful information if you want the bar always on screen,
    // whereas a block of dirt never is.
    bool visible() const {
        const auto held = sdk::GameSDK::get().heldItem();
        if (!held.valid || held.maxDurability <= 0) return false;
        if (settingBool("hideundamaged", true) && held.damage <= 0) return false;
        return true;
    }

    float heldFraction() const {
        return sdk::GameSDK::get().heldItem().durabilityFraction();
    }

    ui::Color barColor(float fraction) const {
        if (!settingBool("gradient", true)) return textColor();

        // Three bands rather than a continuous ramp: the point of the colour
        // is to be recognised at a glance, and a smooth green-to-red gradient
        // spends most of its range in muddy in-between shades that read as
        // neither.
        if (fraction <= 0.15f) return ui::Color::rgba(0xFFF87171);   // about to break
        if (fraction <= 0.40f) return ui::Color::rgba(0xFFFBBF24);   // worth repairing
        return ui::Color::rgba(0xFF4ADE80);
    }
};

GLACIER_MODULE(ToolDurability);

} // namespace glacier
