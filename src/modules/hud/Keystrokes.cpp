#include "Keystrokes.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include <imgui.h>
#include <Windows.h>

namespace Glacier::modules {

Keystrokes::Keystrokes()
    : Module("keystrokes", "Keystrokes", "WASD + mouse keys overlay", Category::HUD),
      scale_    (addSetting<FloatSetting>("scale",    "Scale",     "Pixel size multiplier", 1.0f, 0.5f, 2.5f, 0.05f)),
      showMouse_(addSetting<BoolSetting> ("show_mouse","Show Mouse","Display LMB / RMB row",  true)),
      accent_   (addSetting<ColorSetting>("accent",   "Accent",    "Pressed-key colour",     COL_ACCENT)),
      posX_     (addSetting<FloatSetting>("pos_x",    "X",         "Horizontal anchor (px)", 30.f, 0.f, 4000.f, 1.f)),
      posY_     (addSetting<FloatSetting>("pos_y",    "Y",         "Vertical anchor (px)",   220.f, 0.f, 4000.f, 1.f))
{
    EventBus::get().listen<RenderHUDEvent, &Keystrokes::onRender>(this);
}

static bool isDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

static void key(ImDrawList* dl, ImVec2 p, float k, const char* lbl, bool down, unsigned accent) {
    float s = 28.f * k;
    auto bg = down ? accent : COL_BG_PANEL;
    auto fg = down ? COL_TEXT : COL_TEXT_DIM;
    Draw::panel(dl, p, ImVec2(s, s), bg, ROUND_SM);
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(p.x + (s - ts.x) * 0.5f, p.y + (s - ts.y) * 0.5f),
                Draw::argbToImU32(fg), lbl);
}

void Keystrokes::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;

    float k = scale_.get();
    float s = 28.f * k, gap = 4.f * k;
    ImVec2 origin{ posX_.get(), posY_.get() };

    // W centred above ASD
    key(e.drawList, ImVec2(origin.x + s + gap, origin.y),               k, "W", isDown('W'), accent_.get());
    key(e.drawList, ImVec2(origin.x,            origin.y + s + gap),    k, "A", isDown('A'), accent_.get());
    key(e.drawList, ImVec2(origin.x + s + gap,  origin.y + s + gap),    k, "S", isDown('S'), accent_.get());
    key(e.drawList, ImVec2(origin.x + 2*(s+gap),origin.y + s + gap),    k, "D", isDown('D'), accent_.get());

    if (showMouse_.get()) {
        float row = origin.y + 2 * (s + gap);
        float wide = (s * 1.5f);
        Draw::panel(e.drawList, ImVec2(origin.x, row), ImVec2(wide, s),
                    isDown(VK_LBUTTON) ? accent_.get() : COL_BG_PANEL, ROUND_SM);
        ImVec2 ts1 = ImGui::CalcTextSize("LMB");
        e.drawList->AddText(ImVec2(origin.x + (wide - ts1.x) * 0.5f, row + (s - ts1.y) * 0.5f),
                            Draw::argbToImU32(isDown(VK_LBUTTON) ? COL_TEXT : COL_TEXT_DIM), "LMB");

        Draw::panel(e.drawList, ImVec2(origin.x + wide + gap, row), ImVec2(wide, s),
                    isDown(VK_RBUTTON) ? accent_.get() : COL_BG_PANEL, ROUND_SM);
        ImVec2 ts2 = ImGui::CalcTextSize("RMB");
        e.drawList->AddText(ImVec2(origin.x + wide + gap + (wide - ts2.x) * 0.5f, row + (s - ts2.y) * 0.5f),
                            Draw::argbToImU32(isDown(VK_RBUTTON) ? COL_TEXT : COL_TEXT_DIM), "RMB");
    }
}

} // namespace Glacier::modules
