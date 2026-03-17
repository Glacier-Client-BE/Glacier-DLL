#include "ClickStatsDashboard.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>
ClickStatsDashboard::ClickStatsDashboard()
    : ModuleBase("Click Stats","Detailed click statistics dashboard",ICON_FA_CHART_BAR,ModuleCategory::HUD,200.f,380.f)
{
    m_settings.defineFloat("width", "Panel Width", 170.f,100.f,300.f);
    m_settings.defineFloat("bgAlpha","BG Alpha",   0.88f,0.f,1.f);
    m_settings.defineBool ("showPeak","Show Peak CPS",true);
    m_settings.defineBool ("showTotal","Show Total Clicks",true);
    m_settings.defineInt  ("totalClicks","Total (session)",0,0,9999999);
}
void ClickStatsDashboard::onRenderImGui(){
    auto now=Clock::now();
    bool cur=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    if(cur&&!m_pLMB){m_clicks.push_back(now);m_settings.set("totalClicks",m_settings.getInt("totalClicks")+1);}
    m_pLMB=cur;
    auto cut=now-std::chrono::seconds(1);
    while(!m_clicks.empty()&&m_clicks.front()<cut)m_clicks.pop_front();
    int cps=(int)m_clicks.size(); if(cps>m_peakCPS)m_peakCPS=(float)cps;

    float pw=m_settings.getFloat("width"), ba=m_settings.getFloat("bgAlpha");
    float ph=30.f+(m_settings.getBool("showPeak")?16.f:0.f)+(m_settings.getBool("showTotal")?16.f:0.f)+8.f;
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always); ImGui::SetNextWindowSize({pw,ph}); ImGui::SetNextWindowBgAlpha(ba);
    ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.137f,0.153f,0.165f,ba));
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.447f,.537f,.855f,.4f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{8,6}); ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,6);
    if(ImGui::Begin("##csd",nullptr,f)){
        if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImDrawList* dl=ImGui::GetWindowDrawList(); ImVec2 bp=ImGui::GetWindowPos(); bp.x+=8;bp.y+=6;
        float oy=0;
        char buf[48];
        snprintf(buf,sizeof(buf),"CPS: %d",cps);
        dl->AddText(nullptr,14.f,{bp.x,bp.y+oy},IM_COL32(114,137,218,255),buf); oy+=18;
        if(m_settings.getBool("showPeak")){ snprintf(buf,sizeof(buf),"Peak: %.0f",m_peakCPS);
            dl->AddText(nullptr,11.f,{bp.x,bp.y+oy},IM_COL32(255,200,40,220),buf); oy+=14;}
        if(m_settings.getBool("showTotal")){ snprintf(buf,sizeof(buf),"Total: %d",m_settings.getInt("totalClicks"));
            dl->AddText(nullptr,11.f,{bp.x,bp.y+oy},IM_COL32(153,170,181,200),buf);}
    }
    ImGui::End(); ImGui::PopStyleVar(2); ImGui::PopStyleColor(2);
}
