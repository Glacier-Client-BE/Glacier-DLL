#include "ChunkMap.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>

ChunkMap::ChunkMap()
    : ModuleBase("Chunk Map","2D chunk grid with player dot",
                 "chunkmap", ModuleCategory::HUD, 10.f, 10.f)
{
    m_settings.defineInt  ("radius","Radius",   2, 1, 5);
    m_settings.defineFloat("cell",  "Cell Size",60.f,20.f,150.f);
    m_settings.defineBool ("coords","Coords",   true);
    m_settings.defineFloat("scale", "Scale",    1.f, 0.5f, 2.f);
}
void ChunkMap::onRender(ImDrawList* dl) {
    auto* lp=getLocalPlayer();
    Vec3 pos=lp?lp->getPosition():Vec3{0,0,0};
    float sc=m_settings.getFloat("scale");
    int   rad=m_settings.getInt("radius");
    float cs=m_settings.getFloat("cell")*sc;
    bool  showCoords=m_settings.getBool("coords");
    int   pcx=(int)floorf(pos.x/16.f), pcz=(int)floorf(pos.z/16.f);
    float diam=cs*(rad*2+1);
    float wx=m_pos.x, wy=m_pos.y;
    dl->AddRectFilled({wx,wy},{wx+diam,wy+diam},HUDStyle::BG,HUDStyle::BG_ROUND);
    dl->AddRect({wx,wy},{wx+diam,wy+diam},HUDStyle::BORDER,HUDStyle::BG_ROUND,0,1.2f);
    for (int dz=-rad;dz<=rad;dz++) for (int dx=-rad;dx<=rad;dx++) {
        float cx=wx+(dx+rad)*cs, cy=wy+(dz+rad)*cs;
        if (dx==0&&dz==0) dl->AddRectFilled({cx,cy},{cx+cs,cy+cs},IM_COL32(114,137,218,35),3.f);
        dl->AddRect({cx,cy},{cx+cs,cy+cs},HUDStyle::BORDER,0.f,0,1.f);
        if (showCoords) {
            char buf[20]; snprintf(buf,sizeof(buf),"%d,%d",pcx+dx,pcz+dz);
            ImVec2 ts=ImGui::CalcTextSize(buf); float fss=9.f*sc;
            dl->AddText(nullptr,fss,{cx+(cs-ts.x*fss/ImGui::GetFontSize())*0.5f,cy+(cs-fss)*0.5f},
                HUDStyle::BORDER,buf);
        }
    }
    float offX=fmodf(pos.x,16.f)/16.f, offZ=fmodf(pos.z,16.f)/16.f;
    if (offX<0.f) offX+=1.f; if (offZ<0.f) offZ+=1.f;
    float dotX=wx+rad*cs+offX*cs, dotZ=wy+rad*cs+offZ*cs;
    dl->AddCircleFilled({dotX,dotZ},4.5f*sc,HUDStyle::WHITE);
    dl->AddCircle({dotX,dotZ},4.5f*sc,HUDStyle::SHADOW,0,2.f);
}
