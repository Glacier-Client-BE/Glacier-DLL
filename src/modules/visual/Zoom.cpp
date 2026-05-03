#include "Zoom.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include <imgui.h>
#include <Windows.h>

namespace Glacier::modules {

Zoom::Zoom()
    : Module("zoom", "Zoom", "Hold to narrow FOV", Category::Visual, 'C'),
      factor_ (addSetting<FloatSetting>  ("factor", "Factor", "FOV multiplier when held", 0.4f, 0.1f, 0.95f, 0.025f)),
      smooth_ (addSetting<BoolSetting>   ("smooth", "Smooth", "Lerp the transition",      true)),
      holdKey_(addSetting<KeybindSetting>("hold_key","Hold Key","Hold to zoom",           'C'))
{
    EventBus::get().listen<KeyEvent,        &Zoom::onKey>(this);
    EventBus::get().listen<RenderHUDEvent,  &Zoom::onRender>(this);
}

void Zoom::onKey(KeyEvent& k) {
    if (k.consumedByGui) return;
    if (k.vkey == holdKey_.vkey()) holding_ = k.down;
}

void Zoom::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;
    float target = holding_ ? factor_.get() : 1.0f;
    if (smooth_.get()) scroll_ += (target - scroll_) * 0.18f;
    else               scroll_  = target;

    if (std::abs(scroll_ - 1.f) < 1e-3f) return;

    // Visual: subtle vignette + corner brackets so the user sees zoom is
    // active even before the FOV hook is in.
    float pct = 1.f - scroll_;
    auto a = static_cast<unsigned>(pct * 60.f);
    e.drawList->AddRectFilledMultiColor(
        ImVec2(0, 0), ImVec2(e.width, e.height),
        Draw::argbToImU32((a << 24) | 0x000000), Draw::argbToImU32((a << 24) | 0x000000),
        0, 0);
    char b[32];
    std::snprintf(b, sizeof(b), "x%.2f", scroll_);
    Draw::chip(e.drawList, ImVec2(e.width * 0.5f - 28.f, e.height * 0.5f + 30.f), b, COL_ACCENT);
}

} // namespace Glacier::modules
