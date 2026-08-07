#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"
#include "../../sdk/ItemRendering.h"

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
            const float w = r.measureText(msg, size, false);
            r.drawText(msg, ui::Rect{ x, origin.y, w + 4.0f, cell },
                       textColor().withAlpha(0.5f), size);
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

        // The icon is drawn by the game, one pass earlier and therefore *under*
        // this overlay — so when icons are available the slot gets an outline
        // only. Filling it, even at a third alpha, would wash out the very
        // thing the fill used to stand in for.
        if (!items.available()) {
            r.fillRoundedRect(box, 3.0f, ui::Color::rgba(0x55101418));
        }
        r.strokeRoundedRect(box, 3.0f, ui::Color::rgba(0x445A6070), 1.0f);

        if (!stack.valid) return;

        // Ask the game to draw the real item here. Inset slightly so the icon
        // doesn't touch the outline, and leave the bottom strip clear for the
        // durability bar.
        if (items.available()) {
            const float inset = cell * 0.10f;
            items.submit(sdk::ItemDraw{ ref, x + inset, y + inset,
                                        cell - inset * 2.0f, 1.0f });
        }

        if (settingBool("bars", true) && stack.maxDurability > 0) {
            const float frac = stack.durabilityFraction();
            const ui::Rect track{ x + 3.0f, box.bottom() - 6.0f, cell - 6.0f, 3.0f };
            r.fillRect(track, ui::Color::rgba(0x99000000));
            r.fillRect(ui::Rect{ track.x, track.y, track.w * frac, track.h },
                       durabilityColor(frac));
        }

        if (settingBool("counts", true) && stack.count > 1) {
            r.drawText(std::to_string(stack.count),
                       ui::Rect{ x, y + cell * 0.18f, cell - 3.0f, cell * 0.5f },
                       textColor(), cell * 0.38f, ui::TextAlign::Right, true);
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
