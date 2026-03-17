#include "ComboCounter.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

ComboCounter::ComboCounter()
    : ModuleBase("Combo Counter","Tracks consecutive melee hits",
                 ICON_FA_FIRE, ModuleCategory::Combat, 10.f, 110.f)
{
    m_settings.defineFloat("resetTime","Reset Timeout (s)",3.f,1.f,10.f);
    m_settings.defineBool ("showMax",  "Show Max Combo",   true);
    m_settings.defineBool ("shadow",   "Text Shadow",      true);
    m_settings.defineFloat("fontSize", "Font Size",       18.f,10.f,36.f);
    m_settings.defineBool ("colorRamp","Color Ramp",       true);
    m_settings.defineInt  ("highThresh","High Combo At",  20,  5,  50);
    m_settings.defineInt  ("midThresh", "Mid Combo At",   10,  3,  30);
}
void ComboCounter::onEnable(){m_combo=m_maxCombo=0;m_pLMB=false;}
void ComboCounter::registerHit(){m_combo++;if(m_combo>m_maxCombo)m_maxCombo=m_combo;m_lastHit=Clock::now();}
void ComboCounter::resetCombo(){m_combo=0;}

void ComboCounter::onRenderImGui(){
    bool cur=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    if(cur&&!m_pLMB)registerHit(); m_pLMB=cur;
    if(m_combo>0&&std::chrono::duration<float>(Clock::now()-m_lastHit).count()>m_settings.getFloat("resetTime"))
        resetCombo();
    float fs=m_settings.getFloat("fontSize"); bool shad=m_settings.getBool("shadow");
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);ImGui::SetNextWindowSize({0,0});
    ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{4,4});ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    if(ImGui::Begin("##combo",nullptr,f)){
        if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImDrawList* dl=ImGui::GetWindowDrawList();ImVec2 b=ImGui::GetWindowPos();
        ImU32 col=IM_COL32(255,255,255,255);
        if(m_settings.getBool("colorRamp")){
            if(m_combo>=(int)m_settings.getInt("highThresh"))col=IM_COL32(255,80,80,255);
            else if(m_combo>=(int)m_settings.getInt("midThresh"))col=IM_COL32(255,200,40,255);
        }
        char buf[32]; snprintf(buf,sizeof(buf),"Combo: %d",m_combo);
        if(shad)dl->AddText(nullptr,fs,{b.x+1,b.y+1},IM_COL32(0,0,0,160),buf);
        dl->AddText(nullptr,fs,b,col,buf);
        if(m_settings.getBool("showMax")){
            snprintf(buf,sizeof(buf),"Best:  %d",m_maxCombo);
            if(shad)dl->AddText(nullptr,fs*.75f,{b.x+1,b.y+fs+5},IM_COL32(0,0,0,120),buf);
            dl->AddText(nullptr,fs*.75f,{b.x,b.y+fs+4},IM_COL32(153,170,181,220),buf);
        }
    }
    ImGui::End();ImGui::PopStyleVar(2);
}
