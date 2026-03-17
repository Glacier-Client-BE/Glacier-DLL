#include "Clock.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <Windows.h>
#include <cstdio>
Clock::Clock()
    : ModuleBase("Clock","Displays local system time",ICON_FA_CLOCK,ModuleCategory::HUD,10.f,440.f)
{
    m_settings.defineBool ("show24h",  "24-Hour Format",   true);
    m_settings.defineBool ("showDate", "Show Date",        false);
    m_settings.defineBool ("showSecs", "Show Seconds",     true);
    m_settings.defineBool ("shadow",   "Shadow",           true);
    m_settings.defineFloat("fontSize", "Font Size",       14.f, 8.f,28.f);
    m_settings.defineFloat("bgAlpha",  "BG Alpha",        0.f,  0.f, 1.f);
}
void Clock::onRenderImGui() {
    SYSTEMTIME st; GetLocalTime(&st);
    char timeBuf[32], dateBuf[32];
    bool h24=m_settings.getBool("show24h"), secs=m_settings.getBool("showSecs");
    if(h24) snprintf(timeBuf,sizeof(timeBuf),secs?"%02d:%02d:%02d":"%02d:%02d",st.wHour,st.wMinute,st.wSecond);
    else    snprintf(timeBuf,sizeof(timeBuf),secs?"%02d:%02d:%02d %s":"%02d:%02d %s",
                     st.wHour%12?st.wHour%12:12,st.wMinute,st.wSecond,st.wHour<12?"AM":"PM");
    snprintf(dateBuf,sizeof(dateBuf),"%04d-%02d-%02d",st.wYear,st.wMonth,st.wDay);
    float fs=m_settings.getFloat("fontSize"), ba=m_settings.getFloat("bgAlpha");
    bool shad=m_settings.getBool("shadow"), showDate=m_settings.getBool("showDate");
    float boxW=ImGui::CalcTextSize(timeBuf).x+12.f, boxH=fs+(showDate?fs+3.f:0.f)+8.f;
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always); ImGui::SetNextWindowSize({boxW,boxH});
    ImGui::SetNextWindowBgAlpha(ba);
    ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.137f,0.153f,0.165f,ba));
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{6,4});
    if(ImGui::Begin("##clock",nullptr,f)){
        if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImDrawList* dl=ImGui::GetWindowDrawList(); ImVec2 bp=ImGui::GetWindowPos(); bp.x+=6;bp.y+=4;
        if(shad)dl->AddText(nullptr,fs,{bp.x+1,bp.y+1},IM_COL32(0,0,0,160),timeBuf);
        dl->AddText(nullptr,fs,bp,IM_COL32(114,137,218,255),timeBuf);
        if(showDate){ bp.y+=fs+3;
            if(shad)dl->AddText(nullptr,fs*.85f,{bp.x+1,bp.y+1},IM_COL32(0,0,0,120),dateBuf);
            dl->AddText(nullptr,fs*.85f,bp,IM_COL32(153,170,181,200),dateBuf);}
    }
    ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
}
