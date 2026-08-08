#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"
#include "../../sdk/ItemRendering.h"

#include <string>

namespace glacier {

// The full 36-slot inventory — 3 rows of 9 main slots, then the hotbar row —
// laid out the way the vanilla inventory screen does, so it reads as "your
// inventory" rather than an arbitrary grid.
//
// Armor isn't drawn here; Armor HUD already covers it, and duplicating those
// four slots would just be two widgets disagreeing if their settings drift.
//
// Chrome-less for the same reason as Armor HUD and Hotbar HUD: the icon is
// the game's own, drawn a frame behind this overlay, and a box or fill here
// competes with it instead of framing it. The one addition over Hotbar HUD
// is the row gap that separates "main" from "hotbar" — the same visual break
// the vanilla screen uses, just without a background to draw it on.
class InventoryHud final : public HudModule {
public:
    InventoryHud()
        : HudModule("Inventory HUD", "Shows your full inventory",
                    Category::Player, 0,
                    0.30f, 0.55f, 0xFFFFFFFF) {
        addSetting(Setting{ "slot", "Slot size", 22.0f, 14.0f, 40.0f, 1.0f });
        addSetting(Setting{ "gap", "Spacing", 2.0f, 0.0f, 10.0f, 1.0f });
        addSetting(Setting{ "counts", "Stack counts", true });
        addSetting(Setting{ "highlight", "Highlight selected hotbar slot", true });
        addSetting(Setting{ "highlightcolor", "Highlight color",
                            Setting::ColorTag{}, 0xFF4C9AFF });
    }

    ui::Rect measureHudBody(float scale) override {
        const float cell = settingFloat("slot", 22.0f) * scale;
        const float gap  = settingFloat("gap", 2.0f) * scale;
        const float rowGap = gap * 2.5f;   // the main/hotbar break

        const float width  = 9.0f * cell + 8.0f * gap;
        const float height = 4.0f * cell + 2.0f * gap + rowGap;
        return ui::Rect{ 0, 0, width, height };
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        const float cell = settingFloat("slot", 22.0f) * scale;
        const float gap  = settingFloat("gap", 2.0f) * scale;
        const float rowGap = gap * 2.5f;

        auto& sdkRef = sdk::GameSDK::get();
        const auto stacks = sdkRef.inventory();
        const int selected = settingBool("highlight", true) ? sdkRef.selectedHotbarSlot() : -1;

        // Main grid: slots 9..35, three rows of nine, in the same top-to-bottom
        // order the vanilla screen uses.
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 9; ++col) {
                const int slot = 9 + row * 9 + col;
                drawSlot(origin.x + col * (cell + gap), origin.y + row * (cell + gap),
                         cell, stacks[static_cast<std::size_t>(slot)],
                         sdk::ItemRef{ sdk::ItemRef::Source::Inventory, slot }, false);
            }
        }

        // Hotbar row, set off by the wider gap.
        const float hotbarY = origin.y + 3.0f * (cell + gap) - gap + rowGap;
        for (int col = 0; col < 9; ++col) {
            drawSlot(origin.x + col * (cell + gap), hotbarY, cell,
                     stacks[static_cast<std::size_t>(col)],
                     sdk::ItemRef{ sdk::ItemRef::Source::Hotbar, col }, col == selected);
        }

        const float width  = 9.0f * cell + 8.0f * gap;
        const float height = (hotbarY + cell) - origin.y;
        return ui::Rect{ origin.x, origin.y, width, height };
    }

private:
    void drawSlot(float x, float y, float cell, const sdk::ItemStack& stack,
                  const sdk::ItemRef& ref, bool selected) {
        auto& r = ui::Renderer::get();
        auto& items = sdk::ItemRendering::get();
        const ui::Rect box{ x, y, cell, cell };

        if (!items.available()) {
            r.strokeRoundedRect(box, cell * 0.12f, ui::Color::rgba(0x33FFFFFF), 1.0f);
        }

        if (selected) {
            const ui::Rect underline{ x + cell * 0.1f, box.bottom() + 2.0f, cell * 0.8f, 2.0f };
            r.fillRoundedRect(underline, 1.0f,
                              ui::Color::rgba(settingColor("highlightcolor", 0xFF4C9AFF)));
        }

        if (!stack.valid) return;

        if (items.available()) {
            const float inset = cell * 0.06f;
            items.submit(sdk::ItemDraw{ ref, x + inset, y + inset,
                                        cell - inset * 2.0f, 1.0f });
        }

        if (settingBool("counts", true) && stack.count > 1) {
            const std::string text = std::to_string(stack.count);
            const float size = cell * 0.36f;
            const ui::Rect countBox{ x, y + cell * 0.16f, cell - cell * 0.08f, cell * 0.5f };
            r.drawText(text, ui::Rect{ countBox.x + 1.0f, countBox.y + 1.0f, countBox.w, countBox.h },
                       ui::Color{ 0.0f, 0.0f, 0.0f, 0.65f }, size, ui::TextAlign::Right, ui::FontWeight::SemiBold);
            r.drawText(text, countBox, textColor(), size, ui::TextAlign::Right, ui::FontWeight::SemiBold);
        }
    }
};

GLACIER_MODULE(InventoryHud);

} // namespace glacier
