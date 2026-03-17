#include "Keystrokes.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

Keystrokes::Keystrokes()
    : ModuleBase("Keystrokes", "WASD / Space / Mouse key display",
                 ICON_FA_KEYBOARD, ModuleCategory::HUD, 220.f, 200.f)
{
    m_settings.defineFloat("scale",      "Scale",           1.0f, 0.5f, 2.5f);
    m_settings.defineBool ("showSpace",  "Show Spacebar",   true);
    m_settings.defineBool ("showMouse",  "Show Mouse Btns", true);
    m_settings.defineBool ("shadow",     "Shadow",          true);
    m_settings.defineFloat("pressedR",   "Pressed Color R", 72.f, 0.f, 255.f);
    m_settings.defineFloat("pressedG",   "Pressed Color G",103.f, 0.f, 255.f);
    m_settings.defineFloat("pressedB",   "Pressed Color B",218.f, 0.f, 255.f);
    m_settings.defineFloat("rounding",   "Key Rounding",    5.f,  0.f, 12.f);
}

void Keystrokes::drawKey(ImDrawList* dl, const char* lbl, float x, float y, float w, float h, bool pressed) const {
    float r = m_settings.getFloat("rounding");
    ImU32 bg = pressed
        ? IM_COL32((int)m_settings.getFloat("pressedR"),(int)m_settings.getFloat("pressedG"),(int)m_settings.getFloat("pressedB"),230)
        : IM_COL32(44,47,51,210);
    ImU32 border = IM_COL32(114,137,218,180);
    dl->AddRectFilled({x,y},{x+w,y+h}, bg, r);
    dl->AddRect({x,y},{x+w,y+h}, border, r, 0, pressed ? 2.f : 1.f);
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    ImVec2 tp{x+(w-ts.x)*.5f, y+(h-ts.y)*.5f};
    if (m_settings.getBool("shadow")) dl->AddText({tp.x+1.f,tp.y+1.f}, IM_COL32(0,0,0,160), lbl);
    dl->AddText(tp, pressed ? IM_COL32(255,255,255,255) : IM_COL32(153,170,181,200), lbl);
}

void Keystrokes::onRenderImGui() {
    float sc = m_settings.getFloat("scale");
    float cw = 42.f*sc, ch = 38.f*sc, gap = 4.f*sc;
    bool sp = m_settings.getBool("showSpace"), ms = m_settings.getBool("showMouse");
    float tw = cw*3+gap*2;
    float rows = 2.f + (sp?1.f:0.f) + (ms?1.f:0.f);
    float th = ch*rows + gap*(rows-1.f);

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({tw,th}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##ks",nullptr,f)) {
        if (ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImDrawList* dl=ImGui::GetWindowDrawList();
        float ox=m_pos.x,oy=m_pos.y;
        drawKey(dl,"W",ox+cw+gap,oy,cw,ch,GetAsyncKeyState('W')&0x8000); oy+=ch+gap;
        drawKey(dl,"A",ox,oy,cw,ch,GetAsyncKeyState('A')&0x8000);
        drawKey(dl,"S",ox+cw+gap,oy,cw,ch,GetAsyncKeyState('S')&0x8000);
        drawKey(dl,"D",ox+(cw+gap)*2,oy,cw,ch,GetAsyncKeyState('D')&0x8000); oy+=ch+gap;
        if(sp){drawKey(dl,"SPACE",ox,oy,tw,ch,GetAsyncKeyState(VK_SPACE)&0x8000);oy+=ch+gap;}
        if(ms){float half=(tw-gap)*.5f;
            drawKey(dl,"LMB",ox,oy,half,ch,GetAsyncKeyState(VK_LBUTTON)&0x8000);
            drawKey(dl,"RMB",ox+half+gap,oy,half,ch,GetAsyncKeyState(VK_RBUTTON)&0x8000);}
    }
    ImGui::End(); ImGui::PopStyleVar(2);
}
