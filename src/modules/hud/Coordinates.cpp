#include "Coordinates.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include "../../sdk/SDK.h"
#include <imgui.h>

namespace Glacier::modules {

static const char* facingFromYaw(float yaw) {
    yaw = std::fmod(yaw, 360.f); if (yaw < 0) yaw += 360.f;
    static const char* dirs[8] = { "S", "SW", "W", "NW", "N", "NE", "E", "SE" };
    return dirs[static_cast<int>((yaw + 22.5f) / 45.f) & 7];
}

Coordinates::Coordinates()
    : Module("coordinates", "Coordinates", "World position display", Category::HUD),
      position_     (addSetting<EnumSetting>("position",       "Position",      "Corner anchor",
                         std::vector<std::string>{ "Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right" }, 2)),
      showDirection_(addSetting<BoolSetting>("show_direction", "Show Direction","Append cardinal facing",   true)),
      showDimension_(addSetting<BoolSetting>("show_dimension", "Show Dimension","Append world type",        false)),
      accent_       (addSetting<ColorSetting>("accent",        "Accent",        "Bullet colour",            COL_ACCENT))
{
    EventBus::get().listen<RenderHUDEvent, &Coordinates::onRender>(this);
}

void Coordinates::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;

    auto pos = sdk::Game::get().playerPos();
    auto yaw = sdk::Game::get().playerYaw();

    char buf[96];
    if (pos.has_value()) {
        if (showDirection_.get() && yaw.has_value()) {
            std::snprintf(buf, sizeof(buf), "X: %.1f  Y: %.1f  Z: %.1f  [%s]",
                          pos->x, pos->y, pos->z, facingFromYaw(*yaw));
        } else {
            std::snprintf(buf, sizeof(buf), "X: %.1f  Y: %.1f  Z: %.1f",
                          pos->x, pos->y, pos->z);
        }
    } else {
        std::snprintf(buf, sizeof(buf), "X: -  Y: -  Z: -");
    }

    ImVec2 ts = ImGui::CalcTextSize(buf);
    float pad = 10.f, w = ts.x + pad * 2 + 14.f, h = ts.y + pad;
    ImVec2 p;
    switch (position_.index()) {
        case 0: p = { 12.f,                  12.f                   }; break;
        case 1: p = { e.width - w - 12.f,    12.f                   }; break;
        case 2: p = { 12.f,                  e.height - h - 12.f    }; break;
        default:p = { e.width - w - 12.f,    e.height - h - 12.f    }; break;
    }

    Draw::panel(e.drawList, p, ImVec2(w, h), COL_BG_PANEL, ROUND_MD);
    e.drawList->AddCircleFilled(ImVec2(p.x + pad, p.y + h * 0.5f), 4.f,
                                 Draw::argbToImU32(accent_.get()));
    e.drawList->AddText(ImVec2(p.x + pad + 12.f, p.y + (h - ts.y) * 0.5f),
                        Draw::argbToImU32(COL_TEXT), buf);
}

} // namespace Glacier::modules
