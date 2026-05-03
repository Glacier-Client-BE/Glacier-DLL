#include "PlayerList.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include "../../sdk/SDK.h"
#include <imgui.h>

namespace Glacier::modules {

PlayerList::PlayerList()
    : Module("player_list", "Player List", "Tab-style roster", Category::HUD),
      position_(addSetting<EnumSetting>("position", "Position", "Anchor",
                    std::vector<std::string>{ "Top-Right", "Top-Left", "Center" }, 0)),
      showPing_(addSetting<BoolSetting>("show_ping", "Show Ping", "Append latency",       true)),
      width_   (addSetting<FloatSetting>("width",    "Width",     "Panel width (px)",     220.f, 140.f, 480.f, 4.f))
{
    EventBus::get().listen<RenderHUDEvent, &PlayerList::onRender>(this);
}

void PlayerList::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;

    // Until an SDK roster is wired, show a single self-row with the local
    // player name (or a placeholder). This still demonstrates the styling.
    auto self = sdk::Game::get().playerName().value_or(std::string("You"));

    struct Row { std::string name; int ping; };
    std::vector<Row> rows = { Row{ self, 24 } };

    float w   = width_.get();
    float row = 22.f;
    float h   = 32.f + rows.size() * row + 8.f;
    ImVec2 p;
    switch (position_.index()) {
        case 1: p = { 14.f,                      14.f }; break;
        case 2: p = { (e.width - w) * 0.5f,      14.f }; break;
        default:p = { e.width - w - 14.f,        14.f }; break;
    }

    Draw::panel(e.drawList, p, ImVec2(w, h), COL_BG_PANEL, ROUND_MD);
    e.drawList->AddText(ImVec2(p.x + 12.f, p.y + 8.f),
                        Draw::argbToImU32(COL_TEXT_DIM), "Players");

    float y = p.y + 32.f;
    for (auto& r : rows) {
        e.drawList->AddCircleFilled(ImVec2(p.x + 16.f, y + row * 0.5f), 4.f,
                                     Draw::argbToImU32(COL_SUCCESS));
        e.drawList->AddText(ImVec2(p.x + 26.f, y + 4.f),
                            Draw::argbToImU32(COL_TEXT), r.name.c_str());
        if (showPing_.get()) {
            char pb[16]; std::snprintf(pb, sizeof(pb), "%dms", r.ping);
            ImVec2 ts = ImGui::CalcTextSize(pb);
            e.drawList->AddText(ImVec2(p.x + w - ts.x - 12.f, y + 4.f),
                                Draw::argbToImU32(COL_TEXT_DIM), pb);
        }
        y += row;
    }
}

} // namespace Glacier::modules
