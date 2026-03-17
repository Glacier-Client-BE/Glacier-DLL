#include "CPSCounter.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

CPSCounter::CPSCounter()
    : ModuleBase("CPS Counter","Left/right clicks per second",
                 ICON_FA_MOUSE_POINTER, ModuleCategory::HUD, 10.f, 60.f)
{
    m_settings.defineBool ("showRight","Show RMB CPS",  true);
    m_settings.defineBool ("shadow",   "Text Shadow",   true);
    m_settings.defineBool ("showBar",  "Show CPS Bar",  true);
    m_settings.defineFloat("fontSize", "Font Size",    16.f, 8.f, 32.f);
    m_settings.defineFloat("barWidth", "Bar Width",    80.f,30.f,200.f);
    m_settings.defineInt  ("maxCPS",   "Max CPS Scale", 20,   5,  40);
}
void CPSCounter::onEnable(){m_L.clear();m_R.clear();m_pL=m_pR=false;}

void CPSCounter::onRenderImGui(){
    auto now=Clock::now();
    bool cL=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    bool cR=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
    if(cL&&!m_pL)m_L.push_back(now); if(cR&&!m_pR)m_R.push_back(now);
    m_pL=cL; m_pR=cR;
    auto cut=now-std::chrono::seconds(1);
    while(!m_L.empty()&&m_L.front()<cut)m_L.pop_front();
    while(!m_R.empty()&&m_R.front()<cut)m_R.pop_front();
    int lc=(int)m_L.size(), rc=(int)m_R.size();
    float fs=m_settings.getFloat("fontSize");
    float bw=m_settings.getFloat("barWidth");
    int mx=m_settings.getInt("maxCPS");
    bool showR=m_settings.getBool("showRight"), showBar=m_settings.getBool("showBar"), shad=m_settings.getBool("shadow");

    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::SetNextWindowSize({0,0});
    ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{4,4});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    if(ImGui::Begin("##cps",nullptr,f)){
        if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 base=ImGui::GetWindowPos();
        char buf[32]; float oy=0.f;
        snprintf(buf,sizeof(buf),"LMB: %d CPS",lc);
        if(shad)dl->AddText(nullptr,fs,{base.x+1,base.y+oy+1},IM_COL32(0,0,0,160),buf);
        dl->AddText(nullptr,fs,{base.x,base.y+oy},IM_COL32(114,137,218,255),buf); oy+=fs+2;
        if(showBar){
            float ratio=mx>0?(float)lc/mx:0.f; if(ratio>1)ratio=1;
            dl->AddRectFilled({base.x,base.y+oy},{base.x+bw,base.y+oy+6},IM_COL32(35,39,42,255),3);
            dl->AddRectFilled({base.x,base.y+oy},{base.x+bw*ratio,base.y+oy+6},IM_COL32(114,137,218,255),3);
            oy+=10;
        }
        if(showR){
            snprintf(buf,sizeof(buf),"RMB: %d CPS",rc);
            if(shad)dl->AddText(nullptr,fs,{base.x+1,base.y+oy+1},IM_COL32(0,0,0,160),buf);
            dl->AddText(nullptr,fs,{base.x,base.y+oy},IM_COL32(153,170,181,255),buf); oy+=fs+2;
            if(showBar){
                float ratio=mx>0?(float)rc/mx:0.f; if(ratio>1)ratio=1;
                dl->AddRectFilled({base.x,base.y+oy},{base.x+bw,base.y+oy+6},IM_COL32(35,39,42,255),3);
                dl->AddRectFilled({base.x,base.y+oy},{base.x+bw*ratio,base.y+oy+6},IM_COL32(153,170,181,255),3);
            }
        }
    }
    ImGui::End(); ImGui::PopStyleVar(2);
}
