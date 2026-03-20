#include "SessionInfo.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <Psapi.h>

SessionInfo::SessionInfo()
    : ModuleBase("Session Info","Playtime, memory and FPS",
                 "sessioninfo", ModuleCategory::HUD, 10.f, 590.f)
{
    m_settings.defineBool ("showTime","Session Time",   true);
    m_settings.defineBool ("showMem", "Memory Usage",   true);
    m_settings.defineBool ("showFPS", "Avg FPS",        false);
    m_settings.defineFloat("scale",   "Scale",          1.f, 0.5f, 2.f);
}
void SessionInfo::onEnable() {
    m_start=std::chrono::high_resolution_clock::now();
    m_fpsAccum=0.f; m_fpsCount=0; m_avgFPS=0.f;
}
void SessionInfo::onRenderImGui() {
    float elapsed=std::chrono::duration<float>(std::chrono::high_resolution_clock::now()-m_start).count();
    int h=(int)(elapsed/3600),m_=(int)(elapsed/60)%60,s=(int)elapsed%60;
    m_fpsAccum+=ImGui::GetIO().DeltaTime; m_fpsCount++;
    if (m_fpsAccum>=1.f){ m_avgFPS=(float)m_fpsCount/m_fpsAccum; m_fpsAccum=0.f; m_fpsCount=0; }

    float sc=m_settings.getFloat("scale");
    bool  st=m_settings.getBool("showTime"),sm=m_settings.getBool("showMem"),sf=m_settings.getBool("showFPS");
    int lines=(st?1:0)+(sm?1:0)+(sf?1:0);
    if (!lines) return;

    float fs=HUDStyle::FONT_MID*sc, lh=fs+4.f;
    float panW=165.f*sc, panH=lines*lh+HUDStyle::PAD_Y*2;
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW,panH});
    HUDStyle::push();
    if (ImGui::Begin("##sinfo",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 bp=ImGui::GetWindowPos();
        float bx=bp.x+HUDStyle::PAD_X, oy=HUDStyle::PAD_Y;
        char buf[48];
        if (st) { snprintf(buf,sizeof(buf),"%02d:%02d:%02d",h,m_,s);
            HUDStyle::text(dl,fs,{bx,bp.y+oy},HUDStyle::ACCENT,buf); oy+=lh; }
        if (sm) { PROCESS_MEMORY_COUNTERS p{}; GetProcessMemoryInfo(GetCurrentProcess(),&p,sizeof(p));
            snprintf(buf,sizeof(buf),"%.0f MB",(float)p.WorkingSetSize/1048576.f);
            HUDStyle::text(dl,fs,{bx,bp.y+oy},HUDStyle::GREY,buf,false); oy+=lh; }
        if (sf) { snprintf(buf,sizeof(buf),"%.0f FPS",m_avgFPS);
            HUDStyle::text(dl,fs,{bx,bp.y+oy},HUDStyle::GREEN,buf,false); }
    }
    ImGui::End(); HUDStyle::pop();
}
