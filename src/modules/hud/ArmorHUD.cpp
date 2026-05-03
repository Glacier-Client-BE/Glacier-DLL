#include "ArmorHUD.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include "../../sdk/SDK.h"
#include <imgui.h>

namespace Glacier::modules {

ArmorHUD::ArmorHUD()
    : Module("armor_hud", "Armor HUD", "Equipped armor + durability", Category::HUD),
      position_(addSetting<EnumSetting>("position", "Position", "Anchor",
                    std::vector<std::string>{ "Above Hotbar", "Top-Right", "Bottom-Left" }, 0)),
      vertical_(addSetting<BoolSetting>("vertical", "Vertical", "Stack pieces vertically", false)),
      accent_  (addSetting<ColorSetting>("accent",  "Accent",   "Fill colour",             COL_ACCENT))
{
    EventBus::get().listen<RenderHUDEvent, &ArmorHUD::onRender>(this);
}

void ArmorHUD::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;
    if (!sdk::Game::get().inGame()) {
        // Placeholder slots so the HUD layout is still demonstrable.
    }

    const char* slots[] = { "H", "C", "L", "B" }; // Helm / Chest / Legs / Boots
    int slotCount = 4;

    float w = vertical_.get() ? 36.f : (slotCount * 36.f + (slotCount - 1) * 4.f);
    float h = vertical_.get() ? (slotCount * 36.f + (slotCount - 1) * 4.f) : 36.f;

    ImVec2 p;
    switch (position_.index()) {
        case 1: p = { e.width - w - 14.f, 14.f }; break;
        case 2: p = { 14.f, e.height - h - 14.f }; break;
        default: p = { (e.width - w) * 0.5f, e.height - h - 60.f }; break;
    }

    for (int i = 0; i < slotCount; ++i) {
        ImVec2 sp = vertical_.get()
            ? ImVec2(p.x, p.y + i * 40.f)
            : ImVec2(p.x + i * 40.f, p.y);
        Draw::panel(e.drawList, sp, ImVec2(36.f, 36.f), COL_BG_PANEL, ROUND_SM);
        ImVec2 ts = ImGui::CalcTextSize(slots[i]);
        e.drawList->AddText(ImVec2(sp.x + (36 - ts.x) * 0.5f, sp.y + (36 - ts.y) * 0.5f),
                            Draw::argbToImU32(COL_TEXT_DIM), slots[i]);
        // accent durability bar - placeholder until SDK is wired
        e.drawList->AddRectFilled(
            ImVec2(sp.x + 4.f, sp.y + 32.f),
            ImVec2(sp.x + 32.f, sp.y + 34.f),
            Draw::argbToImU32(accent_.get()), 1.f);
    }
}

} // namespace Glacier::modules
