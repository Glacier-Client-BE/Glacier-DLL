#include "HitColor.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <chrono>
HitColor::HitColor()
    : ModuleBase("Hit Color","Flashes screen when you hit an entity",ICON_FA_BOLT,ModuleCategory::Combat)
{
    m_settings.defineFloat("r",      "Flash R",     255.f,0.f,255.f);
    m_settings.defineFloat("g",      "Flash G",       0.f,0.f,255.f);
    m_settings.defineFloat("b",      "Flash B",       0.f,0.f,255.f);
    m_settings.defineFloat("maxAlpha","Max Alpha",    0.18f,0.f,0.5f);
    m_settings.defineFloat("fadeMs", "Fade Time (ms)",200.f,50.f,600.f);
}
void HitColor::onRender(ImDrawList* dl){
    bool cur=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    if(cur&&!m_prevLMB){m_flash=true;m_lastHit=std::chrono::high_resolution_clock::now();}
    m_prevLMB=cur;
    if(!m_flash) return;
    float elapsed=std::chrono::duration<float>(std::chrono::high_resolution_clock::now()-m_lastHit).count()*1000.f;
    float fade=m_settings.getFloat("fadeMs");
    if(elapsed>fade){m_flash=false;return;}
    float t=1.f-(elapsed/fade);
    float a=m_settings.getFloat("maxAlpha")*t;
    ImGuiIO& io=ImGui::GetIO();
    dl->AddRectFilled({0,0},{io.DisplaySize.x,io.DisplaySize.y},
        IM_COL32((int)m_settings.getFloat("r"),(int)m_settings.getFloat("g"),(int)m_settings.getFloat("b"),(int)(a*255)));
}
