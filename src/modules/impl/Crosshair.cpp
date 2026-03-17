#include "Crosshair.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
Crosshair::Crosshair()
    : ModuleBase("Crosshair","Custom crosshair overlay",ICON_FA_PLUS,ModuleCategory::HUD)
{
    m_settings.defineFloat("size",   "Size",         8.f,  2.f, 30.f);
    m_settings.defineFloat("thick",  "Thickness",    1.5f, 1.f,  5.f);
    m_settings.defineFloat("gap",    "Center Gap",   3.f,  0.f, 15.f);
    m_settings.defineFloat("r",      "Color R",    255.f,  0.f,255.f);
    m_settings.defineFloat("g",      "Color G",    255.f,  0.f,255.f);
    m_settings.defineFloat("b",      "Color B",    255.f,  0.f,255.f);
    m_settings.defineFloat("alpha",  "Alpha",       0.9f,  0.f,  1.f);
    m_settings.defineBool ("dot",    "Center Dot",  false);
    m_settings.defineBool ("outline","Outline",      true);
    m_settings.defineFloat("outAlpha","Outline Alpha",0.5f,0.f,1.f);
}
void Crosshair::onRender(ImDrawList* dl) {
    ImGuiIO& io=ImGui::GetIO();
    float cx=io.DisplaySize.x*.5f, cy=io.DisplaySize.y*.5f;
    float sz=m_settings.getFloat("size"), th=m_settings.getFloat("thick");
    float gap=m_settings.getFloat("gap");
    ImU32 col=IM_COL32((int)m_settings.getFloat("r"),(int)m_settings.getFloat("g"),
                       (int)m_settings.getFloat("b"),(int)(m_settings.getFloat("alpha")*255));
    ImU32 out=IM_COL32(0,0,0,(int)(m_settings.getFloat("outAlpha")*255));
    float ex=th+1.f;
    if(m_settings.getBool("outline")) {
        dl->AddLine({cx-sz-1,cy},{cx-gap-1,cy},out,ex);
        dl->AddLine({cx+gap+1,cy},{cx+sz+1,cy},out,ex);
        dl->AddLine({cx,cy-sz-1},{cx,cy-gap-1},out,ex);
        dl->AddLine({cx,cy+gap+1},{cx,cy+sz+1},out,ex);
    }
    dl->AddLine({cx-sz,cy},{cx-gap,cy},col,th);
    dl->AddLine({cx+gap,cy},{cx+sz,cy},col,th);
    dl->AddLine({cx,cy-sz},{cx,cy-gap},col,th);
    dl->AddLine({cx,cy+gap},{cx,cy+sz},col,th);
    if(m_settings.getBool("dot")) dl->AddCircleFilled({cx,cy},th,col);
}
