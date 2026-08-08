#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"
#include "../../sdk/ItemRendering.h"

#include <string>

namespace glacier {

// All 9 hotbar slots, for when the game's own hotbar is hidden or the player
// wants one that can sit somewhere other than bottom-centre.
//
// Chrome-less for the same reason as Armor HUD (see that file): the icon is
// the game's own, drawn a frame behind and underneath this overlay, so a box
// or fill here would compete with it rather than frame it. The one piece of
// chrome this widget keeps that Armor HUD doesn't is the selected-slot
// highlight — an underline, not a box, since a box wide enough to surround a
// 48px icon at the default scale reads as a background the moment it's lit.
class HotbarHud final : public HudModule {
public:
    HotbarHud()
        : HudModule("Hotbar HUD", "Shows all 9 hotbar slots",
                    Category::Player, 0,
                    0.30f, 0.90f, 0xFFFFFFFF) {
        setIcon(0xF00A);   // fa-table-cells
        addSetting(Setting{ "slot", "Slot size", 28.0f, 18.0f, 56.0f, 1.0f });
        addSetting(Setting{ "gap", "Spacing", 3.0f, 0.0f, 14.0f, 1.0f });
        addSetting(Setting{ "counts", "Stack counts", true });
        addSetting(Setting{ "highlight", "Highlight selected slot", true });
        addSetting(Setting{ "highlightcolor", "Highlight color",
                            Setting::ColorTag{}, 0xFF4C9AFF });
    }

    ui::Rect measureHudBody(float scale) override {
        const float cell = settingFloat("slot", 28.0f) * scale;
        const float gap  = settingFloat("gap", 3.0f) * scale;
        return ui::Rect{ 0, 0, 9.0f * cell + 8.0f * gap, cell };
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        const float cell = settingFloat("slot", 28.0f) * scale;
        const float gap  = settingFloat("gap", 3.0f) * scale;

        auto& sdkRef = sdk::GameSDK::get();
        const auto stacks = sdkRef.hotbar();
        const int selected = settingBool("highlight", true) ? sdkRef.selectedHotbarSlot() : -1;

        float x = origin.x;
        for (int slot = 0; slot < 9; ++slot) {
            drawSlot(x, origin.y, cell, stacks[static_cast<std::size_t>(slot)],
                     sdk::ItemRef{ sdk::ItemRef::Source::Hotbar, slot }, slot == selected);
            x += cell + gap;
        }

        return ui::Rect{ origin.x, origin.y, 9.0f * cell + 8.0f * gap, cell };
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

GLACIER_MODULE(HotbarHud);

} // namespace glacier
