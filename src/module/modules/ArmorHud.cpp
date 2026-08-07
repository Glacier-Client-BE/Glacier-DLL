#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

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
            for (const auto& stack : sdkRef.armor()) {
                if (!stack.valid && !showEmpty) continue;
                drawSlot(x, origin.y, cell, stack);
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
                drawSlot(x, origin.y, cell, held);
                x += cell + gap;
            }
        }

        const float width = (x > origin.x) ? (x - origin.x - gap) : 0.0f;
        return ui::Rect{ origin.x, origin.y, width, cell };
    }

private:
    void drawSlot(float x, float y, float cell, const sdk::ItemStack& stack) {
        auto& r = ui::Renderer::get();
        const ui::Rect box{ x, y, cell, cell };

        r.fillRoundedRect(box, 3.0f, ui::Color::rgba(0x55101418));
        r.strokeRoundedRect(box, 3.0f, ui::Color::rgba(0x445A6070), 1.0f);

        if (!stack.valid) return;

        // No item icons: the texture atlas isn't reachable from the overlay's
        // private device. A durability bar plus the count is the useful part
        // anyway, and a wrong icon would be worse than none.
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
