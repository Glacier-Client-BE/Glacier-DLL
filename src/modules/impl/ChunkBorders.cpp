#include "ChunkBorders.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cmath>

ChunkBorders::ChunkBorders()
    : ModuleBase("Chunk Borders","Renders chunk boundary lines",ICON_FA_TH,ModuleCategory::Visual)
{
    m_settings.defineFloat("r",    "Color R",   114.f, 0.f, 255.f);
    m_settings.defineFloat("g",    "Color G",   137.f, 0.f, 255.f);
    m_settings.defineFloat("b",    "Color B",   218.f, 0.f, 255.f);
    m_settings.defineFloat("alpha","Alpha",       0.6f, 0.f, 1.f);
    m_settings.defineFloat("thick","Thickness",   1.5f, 0.5f, 4.f);
    m_settings.defineInt  ("radius","Radius (chunks)",2,   1,   8);
    m_settings.defineBool ("showCoords","Show Chunk Coords",true);
}

void ChunkBorders::onRender(ImDrawList* dl) {
    auto* lp = getLocalPlayer();
    if (!lp) return;

    const float* vp = ViewProjectionCache::get().get4x4();
    Vec3 pos = lp->getPosition();

    int  rad  = m_settings.getInt("radius");
    ImU32 col = IM_COL32((int)m_settings.getFloat("r"),(int)m_settings.getFloat("g"),
                          (int)m_settings.getFloat("b"),(int)(m_settings.getFloat("alpha")*255));
    float thick = m_settings.getFloat("thick");

    int pcx = (int)floorf(pos.x / 16.f);
    int pcz = (int)floorf(pos.z / 16.f);

    float py = pos.y; // draw lines at player height

    for (int cx = pcx - rad; cx <= pcx + rad; cx++) {
        for (int cz = pcz - rad; cz <= pcz + rad; cz++) {
            float wx0 = (float)(cx  ) * 16.f;
            float wx1 = (float)(cx+1) * 16.f;
            float wz0 = (float)(cz  ) * 16.f;
            float wz1 = (float)(cz+1) * 16.f;

            // 4 edges of this chunk at player Y
            Vec3 corners[4] = {{wx0,py,wz0},{wx1,py,wz0},{wx1,py,wz1},{wx0,py,wz1}};
            ImVec2 sc[4]; bool vis[4];
            for (int i = 0; i < 4; i++) vis[i] = worldToScreen(corners[i], sc[i], vp);

            for (int i = 0; i < 4; i++) {
                int j = (i+1)%4;
                if (vis[i] && vis[j]) dl->AddLine(sc[i], sc[j], col, thick);
            }

            if (m_settings.getBool("showCoords")) {
                Vec3 center = {wx0+8.f, py, wz0+8.f};
                ImVec2 scCenter;
                if (worldToScreen(center, scCenter, vp)) {
                    char buf[20]; snprintf(buf,sizeof(buf),"%d,%d",cx,cz);
                    ImVec2 ts = ImGui::CalcTextSize(buf);
                    dl->AddText(nullptr,10.f,{scCenter.x-ts.x*.5f,scCenter.y-5.f},
                                IM_COL32((int)m_settings.getFloat("r"),(int)m_settings.getFloat("g"),
                                         (int)m_settings.getFloat("b"),140), buf);
                }
            }
        }
    }
}
