#include "KillCounter.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>

KillCounter::KillCounter()
    : ModuleBase("Kill Counter","Kills, deaths, KDR and streak",
                 "killcounter", ModuleCategory::Combat, 420.f, 10.f)
{
    m_settings.defineBool ("showDeaths","Deaths",      true);
    m_settings.defineBool ("showKDR",  "KDR",          true);
    m_settings.defineBool ("showStreak","Streak",       true);
    m_settings.defineBool ("resetLoad","Reset on Enable",true);
    m_settings.defineFloat("scale",    "Scale",        1.f, 0.5f, 2.f);
    m_settings.defineInt  ("streakHi", "Streak Hi ≥",  3,   2,  20);
}
void KillCounter::onEnable() {
    if (m_settings.getBool("resetLoad")) m_kills=m_deaths=m_streak=m_maxStreak=0;
}
void KillCounter::addKill() {
    m_kills++; m_streak++;
    if (m_streak>m_maxStreak) m_maxStreak=m_streak;
    m_lastKill=std::chrono::high_resolution_clock::now();
}
void KillCounter::addDeath() { m_deaths++; m_streak=0; }
void KillCounter::onTick() {
    if (m_streak>0) {
        float e=std::chrono::duration<float>(std::chrono::high_resolution_clock::now()-m_lastKill).count();
        if (e>15.f) m_streak=0;
    }
}
void KillCounter::onRenderImGui() {
    float sc  = m_settings.getFloat("scale");
    float kdr = m_deaths>0?(float)m_kills/m_deaths:(float)m_kills;
    bool  sD  = m_settings.getBool("showDeaths");
    bool  sK  = m_settings.getBool("showKDR");
    bool  sS  = m_settings.getBool("showStreak");
    int   hi  = m_settings.getInt("streakHi");
    float fs  = HUDStyle::FONT_BIG*sc, fss=HUDStyle::FONT_SMALL*sc;
    float lh  = fss+4.f;
    float panW=110.f*sc;
    float panH=fs+(sD?lh:0.f)+(sK?lh:0.f)+(sS&&m_streak>0?lh:0.f)+HUDStyle::PAD_Y*2;
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW,panH});
    HUDStyle::push();
    if (ImGui::Begin("##kills",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 bp=ImGui::GetWindowPos();
        float bx=bp.x+HUDStyle::PAD_X, oy=HUDStyle::PAD_Y;
        char buf[32];
        snprintf(buf,sizeof(buf),"%d Kills",m_kills);
        HUDStyle::text(dl,fs,{bx,bp.y+oy},HUDStyle::GREEN,buf);
        oy+=fs+2.f;
        if (sD) { snprintf(buf,sizeof(buf),"%d Deaths",m_deaths);
            HUDStyle::text(dl,fss,{bx,bp.y+oy},HUDStyle::RED,buf,false); oy+=lh; }
        if (sK) { snprintf(buf,sizeof(buf),"KDR %.2f",kdr);
            ImU32 kc=kdr>=2.f?HUDStyle::GREEN:kdr>=1.f?HUDStyle::YELLOW:HUDStyle::RED;
            HUDStyle::text(dl,fss,{bx,bp.y+oy},kc,buf,false); oy+=lh; }
        if (sS&&m_streak>0) {
            bool hot=m_streak>=hi;
            float pulse=hot?0.5f+0.5f*sinf((float)ImGui::GetTime()*5.f):1.f;
            snprintf(buf,sizeof(buf),"%d Streak",m_streak);
            ImU32 sc2=hot?IM_COL32(237,(int)(70+120*pulse),40,255):HUDStyle::YELLOW;
            HUDStyle::text(dl,fss,{bx,bp.y+oy},sc2,buf,false);
        }
    }
    ImGui::End(); HUDStyle::pop();
}
