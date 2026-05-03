#include "FPSCounter.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include <imgui.h>
#include <chrono>

namespace Glacier::modules {

FPSCounter::FPSCounter()
    : Module("fps_counter", "FPS Counter", "Smoothed FPS readout", Category::HUD),
      position_(addSetting<EnumSetting>("position", "Position", "Corner anchor",
                    std::vector<std::string>{ "Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right" }, 1)),
      graph_   (addSetting<BoolSetting>("graph",    "Show Graph", "Display a 2-second sparkline", true)),
      accent_  (addSetting<ColorSetting>("accent",  "Accent",     "Foreground accent",            COL_ACCENT))
{
    EventBus::get().listen<RenderHUDEvent, &FPSCounter::onRender>(this);
}

void FPSCounter::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;

    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto dt  = std::chrono::duration<float>(now - last).count();
    last = now;
    if (dt > 1e-5f) {
        float instant = 1.f / dt;
        fps_ = fps_ * 0.92f + instant * 0.08f;
        hist_[head_] = instant;
        head_ = (head_ + 1) % 120;
    }

    char label[32];
    std::snprintf(label, sizeof(label), "%d FPS", static_cast<int>(fps_));
    ImVec2 ts = ImGui::CalcTextSize(label);

    float pad = 10.f;
    float w   = ts.x + pad * 2 + 16.f;
    float h   = ts.y + pad + (graph_.get() ? 22.f : 0.f);
    ImVec2 p;
    switch (position_.index()) {
        case 0: p = { 12.f,            12.f            }; break;
        case 1: p = { e.width - w - 12.f, 12.f         }; break;
        case 2: p = { 12.f,            e.height - h - 12.f }; break;
        default:p = { e.width - w - 12.f, e.height - h - 12.f }; break;
    }

    Draw::panel(e.drawList, p, ImVec2(w, h), COL_BG_PANEL, ROUND_MD);
    e.drawList->AddCircleFilled(ImVec2(p.x + pad, p.y + ts.y * 0.5f + pad * 0.5f), 4.0f,
                                 Draw::argbToImU32(accent_.get()));
    e.drawList->AddText(ImVec2(p.x + pad + 12.f, p.y + pad * 0.5f),
                        Draw::argbToImU32(COL_TEXT), label);

    if (graph_.get()) {
        float gx = p.x + pad;
        float gy = p.y + h - 18.f;
        float gw = w - pad * 2;
        float gh = 14.f;
        float vmin = 1e9f, vmax = 1e-9f;
        for (float v : hist_) { if (v > 0) { vmin = std::min(vmin, v); vmax = std::max(vmax, v); } }
        float rng = std::max(1.f, vmax - vmin);
        for (int i = 0; i < 119; ++i) {
            int a = (head_ + i)     % 120;
            int b = (head_ + i + 1) % 120;
            if (hist_[a] <= 0 || hist_[b] <= 0) continue;
            float xa = gx + (i      / 119.f) * gw;
            float xb = gx + ((i+1)  / 119.f) * gw;
            float ya = gy + gh - ((hist_[a] - vmin) / rng) * gh;
            float yb = gy + gh - ((hist_[b] - vmin) / rng) * gh;
            e.drawList->AddLine(ImVec2(xa, ya), ImVec2(xb, yb),
                                Draw::argbToImU32(accent_.get()), 1.5f);
        }
    }
}

} // namespace Glacier::modules
