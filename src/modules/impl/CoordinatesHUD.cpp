#include "CoordinatesHUD.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>

CoordinatesHUD::CoordinatesHUD()
    : ModuleBase("Coordinates", "XYZ, chunk, dimension and portal coords with colored axes",
                 "coordinateshud", ModuleCategory::HUD, 10.f, 220.f)
{
    m_settings.defineBool ("showXYZ",    "Show XYZ",           true);
    m_settings.defineBool ("showFacing", "Show Facing",         true);
    m_settings.defineBool ("showChunk",  "Show Chunk",          true);
    m_settings.defineBool ("showDim",    "Show Dimension",      true);
    m_settings.defineBool ("showNether", "Portal Coords",       true);
    m_settings.defineBool ("colorAxes",  "Color XYZ Axes",      true);
    m_settings.defineFloat("scale",      "Scale",               1.f, 0.5f, 2.f);
    m_settings.defineInt  ("precision",  "Decimal Places",      1,   0,    3);
}

void CoordinatesHUD::onRenderImGui() {
    float px=0,py=64,pz=0,yaw=0; int dim=0;
    auto* lp = getLocalPlayer();
    if (lp) { Vec3 p=lp->getPosition(); px=p.x;py=p.y;pz=p.z; yaw=lp->getYaw(); dim=lp->getDimension(); }

    while (yaw < 0.f)    yaw += 360.f;
    while (yaw >= 360.f) yaw -= 360.f;

    static const char* dirs[8] = {"S","SW","W","NW","N","NE","E","SE"};
    int di = (int)((yaw + 22.5f) / 45.f) & 7;
    int cx = (int)floorf(px/16.f), cz = (int)floorf(pz/16.f);
    int bx = ((int)floorf(px)%16+16)%16, bz = ((int)floorf(pz)%16+16)%16;

    static const char* dimN[] = {"Overworld","Nether","The End"};
    static const ImU32 dimC[] = { HUDStyle::GREEN, HUDStyle::RED, IM_COL32(180,130,255,255) };
    const char* dimStr = (dim>=0&&dim<=2) ? dimN[dim] : "?";
    ImU32 dc = (dim>=0&&dim<=2) ? dimC[dim] : HUDStyle::GREY;

    bool sXYZ=m_settings.getBool("showXYZ"),sFac=m_settings.getBool("showFacing");
    bool sCh =m_settings.getBool("showChunk"),sDim=m_settings.getBool("showDim");
    bool sNet=m_settings.getBool("showNether");
    bool col =m_settings.getBool("colorAxes");
    float sc =m_settings.getFloat("scale");
    int   pr =m_settings.getInt("precision");

    float fs  = HUDStyle::FONT_MID * sc;
    float lh  = fs + 4.f;

    char xb[32],yb[32],zb[32],fb[48],cb[48],db[32];
    snprintf(xb,sizeof(xb),"X: %.*f",pr,(double)px);
    snprintf(yb,sizeof(yb),"Y: %.*f",pr,(double)py);
    snprintf(zb,sizeof(zb),"Z: %.*f",pr,(double)pz);
    snprintf(fb,sizeof(fb),"%s  %.0f\xc2\xb0",dirs[di],(double)yaw);
    snprintf(cb,sizeof(cb),"Chunk %d,%d  [%d,%d]",cx,cz,bx,bz);
    snprintf(db,sizeof(db),"%s",dimStr);
    char nbuf[64]="";
    if (dim==0) snprintf(nbuf,sizeof(nbuf),"Nether: %.1f, %.1f",(double)(px/8),(double)(pz/8));
    else if (dim==1) snprintf(nbuf,sizeof(nbuf),"OW: %.1f, %.1f",(double)(px*8),(double)(pz*8));

    int lines = (sXYZ?3:0)+(sFac?1:0)+(sCh?1:0)+(sDim?1:0)+(sNet&&nbuf[0]?1:0);
    float panW = 180.f * sc;
    float panH = lines * lh + HUDStyle::PAD_Y * 2;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ panW, panH });
    HUDStyle::push();

    if (ImGui::Begin("##coords", nullptr, HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        p.x += HUDStyle::PAD_X; p.y += HUDStyle::PAD_Y;
        float oy = 0.f;
        auto draw = [&](const char* t, ImU32 c) {
            HUDStyle::text(dl, fs, {p.x, p.y+oy}, c, t); oy += lh;
        };
        if (sXYZ) {
            draw(xb, col ? IM_COL32(255,100,100,255) : HUDStyle::WHITE);
            draw(yb, col ? IM_COL32(100,220,100,255) : HUDStyle::WHITE);
            draw(zb, col ? IM_COL32(100,160,255,255) : HUDStyle::WHITE);
        }
        if (sFac) draw(fb, HUDStyle::YELLOW);
        if (sCh)  draw(cb, HUDStyle::GREY);
        if (sDim) draw(db, dc);
        if (sNet && nbuf[0]) draw(nbuf, IM_COL32(255,150,80,200));
    }
    ImGui::End();
    HUDStyle::pop();
}
