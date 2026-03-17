#include "KillCounter.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

KillCounter::KillCounter()
    : ModuleBase("Kill Counter","Tracks kills, deaths and KDR",
                 ICON_FA_SKULL, ModuleCategory::Combat, 420.f, 10.f)
{
    m_settings.defineBool ("showDeaths",  "Show Deaths",      true);
    m_settings.defineBool ("showKDR",     "Show KDR",         true);
    m_settings.defineBool ("showAssists", "Show Assists",      false);
    m_settings.defineBool ("resetOnLoad", "Reset on Enable",   true);
    m_settings.defineBool ("shadow",      "Text Shadow",       true);
    m_settings.defineFloat("fontSize",    "Font Size",        14.f, 8.f, 28.f);
    m_settings.defineBool ("bgBox",       "BG Box",            true);
    m_settings.defineFloat("bgAlpha",     "BG Alpha",         0.75f, 0.f, 1.f);
}

void KillCounter::onEnable()  { if(m_settings.getBool("resetOnLoad")){m_kills=m_deaths=m_assists=0;} }
void KillCounter::addKill()   { m_kills++; }
void KillCounter::addDeath()  { m_deaths++; }

void KillCounter::onRenderImGui() {
    float fs   = m_settings.getFloat("fontSize");
    bool  shad = m_settings.getBool("shadow");
    bool  bg   = m_settings.getBool("bgBox");
    float ba   = m_settings.getFloat("bgAlpha");
    bool  sd   = m_settings.getBool("showDeaths");
    bool  sk   = m_settings.getBool("showKDR");
    bool  sa   = m_settings.getBool("showAssists");

    float kdr  = m_deaths > 0 ? (float)m_kills/m_deaths : (float)m_kills;
    float lh   = fs + 3.f;
    float rows = 1.f + (sd?1.f:0.f) + (sk?1.f:0.f) + (sa?1.f:0.f);
    float boxH = rows*lh + 8.f, boxW = 120.f;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({boxW, boxH});
    ImGui::SetNextWindowBgAlpha(bg ? ba : 0.f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.137f,0.153f,0.165f,bg?ba:0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.447f,0.537f,0.855f,bg?0.4f:0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{5,4});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,6);

    if (ImGui::Begin("##kills",nullptr,f)) {
        if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 bp=ImGui::GetWindowPos(); bp.x+=5; bp.y+=4;
        float oy=0;
        auto draw=[&](const char* t, ImU32 c){
            if(shad)dl->AddText(nullptr,fs,{bp.x+1,bp.y+oy+1},IM_COL32(0,0,0,150),t);
            dl->AddText(nullptr,fs,{bp.x,bp.y+oy},c,t); oy+=lh;
        };
        char buf[32];
        snprintf(buf,sizeof(buf),ICON_FA_SKULL " Kills: %d",m_kills);
        draw(buf, IM_COL32(72,199,142,255));
        if(sd){ snprintf(buf,sizeof(buf),ICON_FA_HEART_BROKEN " Deaths: %d",m_deaths); draw(buf,IM_COL32(237,70,70,255)); }
        if(sk){ snprintf(buf,sizeof(buf),"KDR: %.2f",kdr); draw(buf,IM_COL32(255,200,40,255)); }
        if(sa){ snprintf(buf,sizeof(buf),"Assists: %d",m_assists); draw(buf,IM_COL32(153,170,181,200)); }
    }
    ImGui::End(); ImGui::PopStyleVar(2); ImGui::PopStyleColor(2);
}
