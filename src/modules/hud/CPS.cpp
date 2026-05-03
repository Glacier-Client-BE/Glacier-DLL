#include "CPS.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include <imgui.h>
#include <Windows.h>

namespace Glacier::modules {

CPSCounter::CPSCounter()
    : Module("cps", "CPS", "Clicks-per-second readout", Category::HUD),
      position_(addSetting<EnumSetting>("position", "Position", "Corner anchor",
                    std::vector<std::string>{ "Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right" }, 1)),
      accent_  (addSetting<ColorSetting>("accent", "Accent", "Foreground", COL_ACCENT))
{
    EventBus::get().listen<RenderHUDEvent, &CPSCounter::onRender>(this);
    EventBus::get().listen<KeyEvent,       &CPSCounter::onKey>(this);
}

void CPSCounter::onKey(KeyEvent& k) {
    if (!k.down || k.consumedByGui) return;
    auto now = Clock::now();
    if (k.vkey == VK_LBUTTON) lmbHits_.push_back(now);
    if (k.vkey == VK_RBUTTON) rmbHits_.push_back(now);
}

void CPSCounter::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;

    auto cutoff = Clock::now() - std::chrono::seconds(1);
    while (!lmbHits_.empty() && lmbHits_.front() < cutoff) lmbHits_.pop_front();
    while (!rmbHits_.empty() && rmbHits_.front() < cutoff) rmbHits_.pop_front();

    char buf[40];
    std::snprintf(buf, sizeof(buf), "%zu | %zu CPS", lmbHits_.size(), rmbHits_.size());
    ImVec2 ts = ImGui::CalcTextSize(buf);

    float pad = 10.f, w = ts.x + pad * 2 + 14.f, h = ts.y + pad;
    ImVec2 p;
    switch (position_.index()) {
        case 0: p = { 12.f,               60.f                  }; break;
        case 1: p = { e.width - w - 12.f, 60.f                  }; break;
        case 2: p = { 12.f,               e.height - h - 60.f   }; break;
        default:p = { e.width - w - 12.f, e.height - h - 60.f   }; break;
    }
    Draw::panel(e.drawList, p, ImVec2(w, h), COL_BG_PANEL, ROUND_MD);
    e.drawList->AddCircleFilled(ImVec2(p.x + pad, p.y + h * 0.5f), 4.f, Draw::argbToImU32(accent_.get()));
    e.drawList->AddText(ImVec2(p.x + pad + 12.f, p.y + (h - ts.y) * 0.5f),
                        Draw::argbToImU32(COL_TEXT), buf);
}

} // namespace Glacier::modules
