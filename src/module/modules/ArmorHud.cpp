#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"
#include "../../sdk/ItemRendering.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace glacier {

// Armor slots with durability, plus the held item.
//
// Armor is read through the game's ECS: there is no fixed Actor->armorContainer
// offset any more, so GameSDK resolves ActorEquipmentComponent out of the entt
// registry. That makes this module the one most sensitive to the entt pin in
// third_party — see src/sdk/EntityComponents.h.
//
// It fails soft in the ordinary cases (not in a world, component absent):
// armor() returns invalid stacks and empty slots are drawn.
//
// The look is deliberately chrome-less, matching Latite's ArmorHUD (see
// reference/latite/src/client/feature/module/modules/hud/ArmorHUD.cpp): no box,
// no outline, no fill — just the item icon the game itself draws, with a
// durability readout and stack count laid directly over it. A backing plate
// was tried and dropped; it competed with the icon underneath rather than
// framing it, which is the opposite of what a plate is for.
//
// `vertical` mirrors Latite's own mode switch (mode_vertical / mode_horizontal
// in ArmorHUD.h). One difference: Latite draws its numeric durability readout
// OUTSIDE the item, in the free space its vertical mode opens up beside each
// row. Glacier's horizontal mode has no such space — the slots sit flush
// against each other — so the readout stays inside the icon's own square in
// both modes rather than existing in one and not the other.
class ArmorHud final : public HudModule {
public:
    ArmorHud()
        : HudModule("Armor HUD", "Shows equipped armor and durability",
                    Category::Player, 0,
                    0.42f, 0.80f, 0xFFFFFFFF) {
        addSetting(Setting{ "slot", "Slot size", 30.0f, 18.0f, 56.0f, 1.0f });
        addSetting(Setting{ "gap", "Spacing", 4.0f, 0.0f, 14.0f, 1.0f });
        addSetting(Setting{ "vertical", "Vertical layout", false });
        addSetting(Setting{ "held", "Include held item", true });
        addSetting(Setting{ "bars", "Durability bars", true });
        addSetting(Setting{ "durabilitytext", "Durability text", false });
        addSetting(Setting{ "durabilitypercent", "Show as percent", true });
        addSetting(Setting{ "counts", "Stack counts", true });
        addSetting(Setting{ "empty", "Show empty slots", true });
        addSetting(Setting{ "notice", "Explain when armor is unreadable", true });
    }

    // Runs one frame ahead of the draw, so the background is sized to the row
    // that is about to be drawn rather than the previous one. Walks the same
    // decisions writeHudBody does; anything that disagrees would show up as a
    // background that does not fit the slots.
    ui::Rect measureHudBody(float scale) override {
        const float cell = settingFloat("slot", 30.0f) * scale;
        const float gap  = settingFloat("gap", 4.0f) * scale;
        const bool  vertical  = settingBool("vertical", false);
        const bool  showEmpty = settingBool("empty", true);

        auto& sdkRef = sdk::GameSDK::get();
        float extent = 0.0f;

        if (sdkRef.armorSupported()) {
            for (const auto& stack : sdkRef.armor()) {
                if (!stack.valid && !showEmpty) continue;
                extent += cell + gap;
            }
        } else if (settingBool("notice", true)) {
            const float size = cell * 0.42f;
            extent += vertical
                ? size * 1.6f + gap   // one line of text, roughly a line-height tall
                : ui::Renderer::get().measureText("Armor unavailable on this build",
                                                  size, ui::FontWeight::Normal) + gap;
        }

        if (settingBool("held", true)) {
            const auto held = sdkRef.heldItem();
            if (held.valid || showEmpty) extent += gap + cell + gap;
        }

        extent = extent > 0.0f ? extent - gap : 0.0f;
        return vertical ? ui::Rect{ 0, 0, cell, extent } : ui::Rect{ 0, 0, extent, cell };
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        const float cell = settingFloat("slot", 30.0f) * scale;
        const float gap  = settingFloat("gap", 4.0f) * scale;
        const bool  vertical  = settingBool("vertical", false);
        const bool  showEmpty = settingBool("empty", true);

        // A single cursor advanced along whichever axis is the layout's
        // primary one, so the two modes share one pass instead of two nearly
        // identical loops that could quietly drift apart.
        float x = origin.x;
        float y = origin.y;
        const auto advance = [&](float amount) {
            if (vertical) y += amount; else x += amount;
        };

        auto& sdkRef = sdk::GameSDK::get();

        if (sdkRef.armorSupported()) {
            int slot = 0;
            for (const auto& stack : sdkRef.armor()) {
                const int thisSlot = slot++;
                if (!stack.valid && !showEmpty) continue;
                drawSlot(x, y, cell, stack, sdk::ItemRef{ sdk::ItemRef::Source::Armor, thisSlot });
                advance(cell + gap);
            }
        } else if (settingBool("notice", true)) {
            // One honest line beats four blank boxes that look like "you have
            // no armor equipped".
            auto& r = ui::Renderer::get();
            const char* msg = "Armor unavailable on this build";
            const float size = cell * 0.42f;
            const float w = r.measureText(msg, size, ui::FontWeight::Normal);
            r.drawText(msg, ui::Rect{ x, y, w + 4.0f, cell },
                       textColor().withAlpha(0.5f), size, ui::TextAlign::Left,
                       ui::FontWeight::Normal, textShadow());
            advance((vertical ? size * 1.6f : w) + gap);
        }

        if (settingBool("held", true)) {
            const auto held = sdkRef.heldItem();
            if (held.valid || showEmpty) {
                advance(gap);   // visual break between armor and hand
                // -1 is "whatever slot is selected", resolved at draw time
                // rather than baked in here — the player can scroll between
                // this frame and the one that draws the icon.
                drawSlot(x, y, cell, held, sdk::ItemRef{ sdk::ItemRef::Source::Hotbar, -1 });
                advance(cell + gap);
            }
        }

        const float extent = vertical ? (y - origin.y) : (x - origin.x);
        const float trimmed = extent > 0.0f ? extent - gap : 0.0f;
        return vertical ? ui::Rect{ origin.x, origin.y, cell, trimmed }
                        : ui::Rect{ origin.x, origin.y, trimmed, cell };
    }

private:
    void drawSlot(float x, float y, float cell, const sdk::ItemStack& stack,
                  const sdk::ItemRef& ref) {
        auto& r = ui::Renderer::get();
        auto& items = sdk::ItemRendering::get();
        const ui::Rect box{ x, y, cell, cell };

        // Our engine's fallback only: when the game's item renderer isn't
        // reachable there is nothing else on screen to say "a slot lives
        // here", so a faint outline stands in for the icon. The references
        // never need this — their DrawUtil can always ask the game to draw an
        // item — so it is drawn only in the one case they never hit.
        if (!items.available()) {
            r.strokeRoundedRect(box, cell * 0.12f, ui::Color::rgba(0x33FFFFFF), 1.0f);
        }

        if (!stack.valid) return;

        if (items.available()) {
            const float inset = cell * 0.06f;
            items.submit(sdk::ItemDraw{ ref, x + inset, y + inset,
                                        cell - inset * 2.0f, 1.0f });
        }

        const bool damageable = stack.maxDurability > 0;
        const float frac = stack.durabilityFraction();

        // A thin colour-coded rail flush with the icon's bottom edge, not a
        // boxed gauge — the same "decoration earns its keep or it's gone" call
        // as dropping the slot outline. A 1px dark stroke under the fill is the
        // only concession to legibility, since there is no backing plate left
        // to guarantee contrast against a bright sky.
        if (settingBool("bars", true) && damageable) {
            const float barH = std::max(2.0f, cell * 0.07f);
            const ui::Rect rail{ x + cell * 0.08f, box.bottom() - barH, cell - cell * 0.16f, barH };
            r.fillRoundedRect(rail, barH * 0.5f, ui::Color::rgba(0x80000000));
            r.fillRoundedRect(ui::Rect{ rail.x, rail.y, rail.w * frac, rail.h },
                               barH * 0.5f, durabilityColor(frac));
        }

        // Bottom-left, mirroring the stack count's bottom-right — the two
        // never collide, and each reads as "the other half" of the same strip.
        if (settingBool("durabilitytext", false) && damageable) {
            char buf[16];
            if (settingBool("durabilitypercent", true)) {
                std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(frac * 100.0f + 0.5f));
            } else {
                std::snprintf(buf, sizeof(buf), "%d", stack.maxDurability - stack.damage);
            }
            drawShadowedText(buf, ui::Rect{ x + cell * 0.04f, y + cell * 0.16f, cell * 0.6f, cell * 0.5f },
                             durabilityColor(frac), cell * 0.32f, ui::TextAlign::Left);
        }

        if (settingBool("counts", true) && stack.count > 1) {
            drawShadowedText(std::to_string(stack.count),
                             ui::Rect{ x, y + cell * 0.16f, cell - cell * 0.08f, cell * 0.5f },
                             textColor(), cell * 0.36f, ui::TextAlign::Right);
        }
    }

    // Straight-onto-game-content text needs its own shadow rather than the
    // shared drawLine() helper: there is no backing plate behind either of
    // these two readouts, so the shadow is load-bearing here, not a default.
    static void drawShadowedText(const std::string& text, const ui::Rect& box,
                                 const ui::Color& color, float size, ui::TextAlign align) {
        auto& r = ui::Renderer::get();
        r.drawText(text, ui::Rect{ box.x + 1.0f, box.y + 1.0f, box.w, box.h },
                   ui::Color{ 0.0f, 0.0f, 0.0f, 0.65f }, size, align, ui::FontWeight::SemiBold);
        r.drawText(text, box, color, size, align, ui::FontWeight::SemiBold);
    }

    static ui::Color durabilityColor(float frac) {
        if (frac > 0.5f)  return ui::Color::rgba(0xFF4ADE80);
        if (frac > 0.25f) return ui::Color::rgba(0xFFFACC15);
        return ui::Color::rgba(0xFFF87171);
    }
};

GLACIER_MODULE(ArmorHud);

} // namespace glacier
