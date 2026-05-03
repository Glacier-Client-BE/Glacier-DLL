#include "Watermark.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include <imgui.h>
#include <chrono>

namespace Glacier::modules {

Watermark::Watermark()
    : Module("watermark", "Watermark", "Glacier branding overlay", Category::HUD),
      showVersion_(addSetting<BoolSetting>("show_version", "Show Version", "Append version after the brand", true)),
      showFps_    (addSetting<BoolSetting>("show_fps",     "Show FPS",     "Append FPS chip",                true)),
      position_   (addSetting<EnumSetting>("position",     "Position",     "Corner anchor",
                       std::vector<std::string>{ "Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right" }, 0)),
      accent_     (addSetting<ColorSetting>("accent", "Accent", "Brand accent colour", COL_ACCENT))
{
    enabled_ = true;
    EventBus::get().listen<RenderHUDEvent, &Watermark::onRender>(this);
}

void Watermark::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;

    // FPS via a smoothed chrono delta.
    static auto lastT = std::chrono::steady_clock::now();
    static float fps = 0.f;
    auto now = std::chrono::steady_clock::now();
    auto dt  = std::chrono::duration<float>(now - lastT).count();
    lastT    = now;
    if (dt > 1e-5f) fps = fps * 0.9f + (1.f / dt) * 0.1f;

    char label[64];
    if (showVersion_.get()) {
        if (showFps_.get()) std::snprintf(label, sizeof(label), "%s v%s   %d FPS", kBrand, kVersion, static_cast<int>(fps));
        else                 std::snprintf(label, sizeof(label), "%s v%s",         kBrand, kVersion);
    } else {
        if (showFps_.get()) std::snprintf(label, sizeof(label), "%s   %d FPS", kBrand, static_cast<int>(fps));
        else                 std::snprintf(label, sizeof(label), "%s", kBrand);
    }

    ImVec2 ts = ImGui::CalcTextSize(label);
    float pad   = 12.f;
    float boxW  = ts.x + pad * 2 + 14.f;
    float boxH  = ts.y + pad * 1.2f;
    ImVec2 pos;
    switch (position_.index()) {
        case 0: pos = { 12.f,                  12.f                  }; break;
        case 1: pos = { e.width - boxW - 12.f, 12.f                  }; break;
        case 2: pos = { 12.f,                  e.height - boxH - 12.f }; break;
        default:pos = { e.width - boxW - 12.f, e.height - boxH - 12.f }; break;
    }

    Draw::panel(e.drawList, pos, ImVec2(boxW, boxH), COL_BG_PANEL, ROUND_MD, true);
    e.drawList->AddCircleFilled(ImVec2(pos.x + pad, pos.y + boxH * 0.5f), 4.5f, Draw::argbToImU32(accent_.get()));
    e.drawList->AddText(ImVec2(pos.x + pad + 12.f, pos.y + (boxH - ts.y) * 0.5f),
                        Draw::argbToImU32(COL_TEXT), label);
}

} // namespace Glacier::modules
