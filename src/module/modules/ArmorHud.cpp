#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"
#include "../../sdk/ItemRendering.h"

#include <algorithm>
#include <string>

namespace glacier {

// Armor slots with durability bars, plus the held item.
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
class ArmorHud final : public HudModule {
public:
    ArmorHud()
        : HudModule("Armor HUD", "Shows equipped armor and durability",
                    Category::Player, 0,
                    0.42f, 0.80f, 0xFFFFFFFF) {
        addSetting(Setting{ "slot", "Slot size", 30.0f, 18.0f, 56.0f, 1.0f });
        addSetting(Setting{ "gap", "Spacing", 4.0f, 0.0f, 14.0f, 1.0f });
        addSetting(Setting{ "held", "Include held item", true });
        addSetting(Setting{ "bars", "Durability bars", true });
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
        const bool  showEmpty = settingBool("empty", true);

        auto& sdkRef = sdk::GameSDK::get();
        float width = 0.0f;

        if (sdkRef.armorSupported()) {
            for (const auto& stack : sdkRef.armor()) {
                if (!stack.valid && !showEmpty) continue;
                width += cell + gap;
            }
        } else if (settingBool("notice", true)) {
            width += ui::Renderer::get().measureText("Armor unavailable on this build",
                                                     cell * 0.42f, ui::FontWeight::Normal) + gap;
        }

        if (settingBool("held", true)) {
            const auto held = sdkRef.heldItem();
            if (held.valid || showEmpty) width += gap + cell + gap;
        }

        return ui::Rect{ 0, 0, width > 0.0f ? width - gap : 0.0f, cell };
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        const float cell = settingFloat("slot", 30.0f) * scale;
        const float gap  = settingFloat("gap", 4.0f) * scale;
        const bool  showEmpty = settingBool("empty", true);

        auto& sdkRef = sdk::GameSDK::get();
        float x = origin.x;

        if (sdkRef.armorSupported()) {
            int slot = 0;
            for (const auto& stack : sdkRef.armor()) {
                const int thisSlot = slot++;
                if (!stack.valid && !showEmpty) continue;
                drawSlot(x, origin.y, cell, stack,
                         sdk::ItemRef{ sdk::ItemRef::Source::Armor, thisSlot });
                x += cell + gap;
            }
        } else if (settingBool("notice", true)) {
            // One honest line beats four blank boxes that look like "you have
            // no armor equipped".
            auto& r = ui::Renderer::get();
            const char* msg = "Armor unavailable on this build";
            const float size = cell * 0.42f;
            const float w = r.measureText(msg, size, ui::FontWeight::Normal);
            r.drawText(msg, ui::Rect{ x, origin.y, w + 4.0f, cell },
                       textColor().withAlpha(0.5f), size, ui::TextAlign::Left,
                       ui::FontWeight::Normal, textShadow());
            x += w + gap;
        }

        if (settingBool("held", true)) {
            const auto held = sdkRef.heldItem();
            if (held.valid || showEmpty) {
                x += gap;   // visual break between armor and hand
                // -1 is "whatever slot is selected", resolved at draw time
                // rather than baked in here — the player can scroll between
                // this frame and the one that draws the icon.
                drawSlot(x, origin.y, cell, held,
                         sdk::ItemRef{ sdk::ItemRef::Source::Hotbar, -1 });
                x += cell + gap;
            }
        }

        const float width = (x > origin.x) ? (x - origin.x - gap) : 0.0f;
        return ui::Rect{ origin.x, origin.y, width, cell };
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

        // A thin colour-coded rail flush with the icon's bottom edge, not a
        // boxed gauge — the same "decoration earns its keep or it's gone" call
        // as dropping the slot outline. A 1px dark stroke under the fill is the
        // only concession to legibility, since there is no backing plate left
        // to guarantee contrast against a bright sky.
        if (settingBool("bars", true) && stack.maxDurability > 0) {
            const float frac = stack.durabilityFraction();
            const float barH = std::max(2.0f, cell * 0.07f);
            const ui::Rect rail{ x + cell * 0.08f, box.bottom() - barH, cell - cell * 0.16f, barH };
            r.fillRoundedRect(rail, barH * 0.5f, ui::Color::rgba(0x80000000));
            r.fillRoundedRect(ui::Rect{ rail.x, rail.y, rail.w * frac, rail.h },
                               barH * 0.5f, durabilityColor(frac));
        }

        if (settingBool("counts", true) && stack.count > 1) {
            const std::string text = std::to_string(stack.count);
            const float size = cell * 0.36f;
            const ui::Rect countBox{ x, y + cell * 0.16f, cell - cell * 0.08f, cell * 0.5f };
            // Manual shadow rather than the shared drawLine() helper: this is
            // the one piece of HUD text drawn straight onto game content with
            // nothing behind it at all, so the shadow is load-bearing, not a
            // style default.
            r.drawText(text, ui::Rect{ countBox.x + 1.0f, countBox.y + 1.0f, countBox.w, countBox.h },
                       ui::Color{ 0.0f, 0.0f, 0.0f, 0.65f }, size, ui::TextAlign::Right, ui::FontWeight::SemiBold);
            r.drawText(text, countBox, textColor(), size, ui::TextAlign::Right, ui::FontWeight::SemiBold);
        }
    }

    static ui::Color durabilityColor(float frac) {
        if (frac > 0.5f)  return ui::Color::rgba(0xFF4ADE80);
        if (frac > 0.25f) return ui::Color::rgba(0xFFFACC15);
        return ui::Color::rgba(0xFFF87171);
    }
};

GLACIER_MODULE(ArmorHud);

} // namespace glacier
