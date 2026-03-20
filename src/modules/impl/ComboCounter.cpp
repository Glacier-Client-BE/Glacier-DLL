// ComboCounter
#include "ComboCounter.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

ComboCounter::ComboCounter()
    : ModuleBase("Combo Counter","Hit streak with fade-out and color ramp",
                 "combocounter", ModuleCategory::Combat, 10.f, 110.f)
{
    m_settings.defineFloat("resetTime","Reset (s)",    3.f,  0.5f,10.f);
    m_settings.defineBool ("showMax",  "Show Max",     true);
    m_settings.defineFloat("scale",    "Scale",        1.f,  0.5f, 2.f);
    m_settings.defineFloat("fadeDelay","Fade Delay (s)",1.5f,0.5f, 5.f);
    m_settings.defineInt  ("highAt",   "High Combo ≥", 20,   5,  50);
    m_settings.defineInt  ("midAt",    "Mid Combo ≥",  10,   3,  30);
}
void ComboCounter::onEnable()  { m_combo=m_max=0; m_pLMB=false; m_alpha=1.f; m_wasDead=false; }
void ComboCounter::onTick() {
    auto* lp=getLocalPlayer(); if (!lp) return;
    bool dead=lp->isDead()||!lp->isAlive();
    if (dead&&!m_wasDead&&m_combo>0) m_combo=0;
    m_wasDead=dead;
}
void ComboCounter::onRenderImGui() {
    bool cur=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    if (cur&&!m_pLMB) { m_combo++; if(m_combo>m_max)m_max=m_combo; m_lastHit=Clock::now(); m_alpha=1.f; }
    m_pLMB=cur;
    float since=m_combo>0?std::chrono::duration<float>(Clock::now()-m_lastHit).count():999.f;
    if (m_combo>0&&since>m_settings.getFloat("resetTime")) m_combo=0;
    float fd=m_settings.getFloat("fadeDelay");
    if (since>fd) { float t=(since-fd)/.8f; m_alpha=1.f-std::min(t,1.f); }
    else m_alpha=1.f;
    if (m_alpha<0.02f||(!m_combo&&!m_max)) return;
    float sc=m_settings.getFloat("scale");
    int   hi=m_settings.getInt("highAt"), mi=m_settings.getInt("midAt");
    ImU32 col = m_combo>=hi ? IM_COL32(237,70,70,(int)(255*m_alpha))
              : m_combo>=mi ? IM_COL32(255,200,40,(int)(255*m_alpha))
                            : IM_COL32(255,255,255,(int)(255*m_alpha));
    char buf[32]; snprintf(buf,sizeof(buf),"%d Combo",m_combo);
    ImVec2 ts=ImGui::CalcTextSize(buf);
    float fs=HUDStyle::FONT_BIG*sc, fss=HUDStyle::FONT_SMALL*sc;
    float panW=ts.x*fs/ImGui::GetFontSize()+HUDStyle::PAD_X*2+20.f;
    float panH=fs+(m_settings.getBool("showMax")?fss+4.f:0.f)+HUDStyle::PAD_Y*2;
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW,panH});
    HUDStyle::push(HUDStyle::BG_ALPHA*m_alpha);
    if (ImGui::Begin("##combo",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 bp=ImGui::GetWindowPos();
        HUDStyle::text(dl,fs,{bp.x+HUDStyle::PAD_X,bp.y+HUDStyle::PAD_Y},col,buf);
        if (m_settings.getBool("showMax")&&m_max>0) {
            char mb[24]; snprintf(mb,sizeof(mb),"Best: %d",m_max);
            HUDStyle::text(dl,fss,{bp.x+HUDStyle::PAD_X,bp.y+HUDStyle::PAD_Y+fs+3.f},
                IM_COL32(180,185,195,(int)(180*m_alpha)),mb,false);
        }
    }
    ImGui::End(); HUDStyle::pop();
}
