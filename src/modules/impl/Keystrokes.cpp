#include "Keystrokes.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>

Keystrokes::Keystrokes()
    : ModuleBase("Keystrokes","WASD + mouse key display with CPS",
                 "keystrokes", ModuleCategory::HUD, 220.f, 200.f)
{
    m_settings.defineBool ("showSpace","Spacebar",      true);
    m_settings.defineBool ("showSneak","Sneak",         true);
    m_settings.defineBool ("showMouse","Mouse Buttons", true);
    m_settings.defineBool ("showCPS",  "CPS on Mouse",  true);
    m_settings.defineFloat("scale",    "Scale",         1.f, 0.5f, 2.5f);
    m_settings.defineFloat("rounding", "Rounding",      6.f, 0.f, 14.f);
    m_settings.defineFloat("pressedR", "Pressed R",    72.f, 0.f,255.f);
    m_settings.defineFloat("pressedG", "Pressed G",   103.f, 0.f,255.f);
    m_settings.defineFloat("pressedB", "Pressed B",   218.f, 0.f,255.f);
}

void Keystrokes::drawKey(ImDrawList* dl, const char* lbl,
                         float x, float y, float w, float h, bool pressed) const {
    float r=m_settings.getFloat("rounding");
    ImU32 bg=pressed
        ? IM_COL32((int)m_settings.getFloat("pressedR"),
                   (int)m_settings.getFloat("pressedG"),
                   (int)m_settings.getFloat("pressedB"), 220)
        : IM_COL32(18,18,18, (int)(255*HUDStyle::BG_ALPHA));
    ImU32 bd=pressed ? HUDStyle::ACCENT : IM_COL32(55,60,68,180);
    dl->AddRectFilled({x,y},{x+w,y+h},bg,r);
    dl->AddRect({x,y},{x+w,y+h},bd,r,0,pressed?2.f:1.f);

    float fs=14.f*m_settings.getFloat("scale");
    ImVec2 ts=ImGui::CalcTextSize(lbl); float sc2=fs/ImGui::GetFontSize();
    ImVec2 tp{x+(w-ts.x*sc2)*0.5f, y+(h-ts.y*sc2)*0.5f};
    HUDStyle::text(dl,fs,tp,pressed?HUDStyle::WHITE:HUDStyle::GREY,lbl,true);
}

void Keystrokes::onRenderImGui() {
    auto now=Clock::now();
    bool cL=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    bool cR=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
    if (cL&&!m_pL) m_cpsL.push_back(now);
    if (cR&&!m_pR) m_cpsR.push_back(now);
    m_pL=cL; m_pR=cR;
    auto cut=now-std::chrono::seconds(1);
    while(!m_cpsL.empty()&&m_cpsL.front()<cut) m_cpsL.pop_front();
    while(!m_cpsR.empty()&&m_cpsR.front()<cut) m_cpsR.pop_front();

    float sc=m_settings.getFloat("scale");
    float cw=44.f*sc, ch=40.f*sc, gap=4.f*sc;
    bool  sp=m_settings.getBool("showSpace"),sn=m_settings.getBool("showSneak");
    bool  ms=m_settings.getBool("showMouse"),cs=m_settings.getBool("showCPS")&&ms;
    float tw=cw*3+gap*2;
    int   extraRows=(sp?1:0)+(sn?1:0)+(ms?1:0);
    float th=ch*(2+extraRows)+gap*(1+extraRows);

    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({tw,th});
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    if (ImGui::Begin("##ks",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        float ox=m_pos.x, oy=m_pos.y;
        drawKey(dl,"W",ox+cw+gap,oy,cw,ch,GetAsyncKeyState('W')&0x8000); oy+=ch+gap;
        drawKey(dl,"A",ox,oy,cw,ch,GetAsyncKeyState('A')&0x8000);
        drawKey(dl,"S",ox+cw+gap,oy,cw,ch,GetAsyncKeyState('S')&0x8000);
        drawKey(dl,"D",ox+(cw+gap)*2,oy,cw,ch,GetAsyncKeyState('D')&0x8000); oy+=ch+gap;
        if (sp){ drawKey(dl,"SPACE",ox,oy,tw,ch,GetAsyncKeyState(VK_SPACE)&0x8000); oy+=ch+gap; }
        if (sn){ drawKey(dl,"SHIFT",ox,oy,tw,ch,GetAsyncKeyState(VK_SHIFT)&0x8000); oy+=ch+gap; }
        if (ms) {
            float half=(tw-gap)*0.5f;
            int lc=(int)m_cpsL.size(), rc=(int)m_cpsR.size();
            char lb[12],rb[12];
            if (cs){ snprintf(lb,sizeof(lb),"LMB\n%d",lc); snprintf(rb,sizeof(rb),"RMB\n%d",rc); }
            else   { snprintf(lb,sizeof(lb),"LMB");         snprintf(rb,sizeof(rb),"RMB"); }
            drawKey(dl,lb,ox,oy,half,ch,cL);
            drawKey(dl,rb,ox+half+gap,oy,half,ch,cR);
        }
    }
    ImGui::End(); ImGui::PopStyleVar(2);
}
