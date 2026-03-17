#include "SessionInfo.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>
SessionInfo::SessionInfo()
    : ModuleBase("Session Info","Shows session playtime and stats",ICON_FA_INFO_CIRCLE,ModuleCategory::HUD,10.f,590.f)
{
    m_settings.defineFloat("fontSize","Font Size",   13.f,8.f,24.f);
    m_settings.defineBool ("shadow",  "Shadow",       true);
    m_settings.defineFloat("bgAlpha","BG Alpha",     0.6f,0.f,1.f);
    m_settings.defineBool ("showTime","Show Playtime",true);
    m_settings.defineBool ("showMem", "Show Memory",  false);
}
void SessionInfo::onEnable(){ m_start=std::chrono::high_resolution_clock::now(); }
void SessionInfo::onRenderImGui(){
    float elapsed=std::chrono::duration<float>(std::chrono::high_resolution_clock::now()-m_start).count();
    int h=(int)(elapsed/3600), m=(int)(elapsed/60)%60, s=(int)elapsed%60;
    float fs=m_settings.getFloat("fontSize"), ba=m_settings.getFloat("bgAlpha");
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always); ImGui::SetNextWindowSize({140,m_settings.getBool("showMem")?fs*2.5f+10.f:fs+8.f});
    ImGui::SetNextWindowBgAlpha(ba);
    ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.137f,0.153f,0.165f,ba));
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{5,4});
    if(ImGui::Begin("##session",nullptr,f)){
        if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImDrawList* dl=ImGui::GetWindowDrawList(); ImVec2 bp=ImGui::GetWindowPos(); bp.x+=5;bp.y+=4;
        if(m_settings.getBool("showTime")){
            char buf[32]; snprintf(buf,sizeof(buf),ICON_FA_CLOCK " %02d:%02d:%02d",h,m,s);
            if(m_settings.getBool("shadow"))dl->AddText(nullptr,fs,{bp.x+1,bp.y+1},IM_COL32(0,0,0,150),buf);
            dl->AddText(nullptr,fs,bp,IM_COL32(114,137,218,255),buf);
        }
        if(m_settings.getBool("showMem")){
            PROCESS_MEMORY_COUNTERS pmc{}; GetProcessMemoryInfo(GetCurrentProcess(),&pmc,sizeof(pmc));
            char buf[32]; snprintf(buf,sizeof(buf),ICON_FA_MICROCHIP " %.0f MB",(float)pmc.WorkingSetSize/1048576.f);
            if(m_settings.getBool("shadow"))dl->AddText(nullptr,fs,{bp.x+1,bp.y+fs+5},IM_COL32(0,0,0,120),buf);
            dl->AddText(nullptr,fs,{bp.x,bp.y+fs+4},IM_COL32(153,170,181,200),buf);
        }
    }
    ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
}
