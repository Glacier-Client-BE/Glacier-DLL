#include "ChunkMap.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cmath>
#include <cstdio>

ChunkMap::ChunkMap()
    : ModuleBase("Chunk Map", "Displays chunk grid with coordinates on the HUD",
                 ICON_FA_TABLE, ModuleCategory::HUD, 10.f, 10.f)
{
    m_settings.defineInt  ("radius",    "Render Radius",   2,    1,  5);
    m_settings.defineFloat("size",      "Cell Size px",   80.f, 30.f, 200.f);
    m_settings.defineFloat("alpha",     "Alpha",           0.9f,  0.f, 1.f);
    m_settings.defineBool ("showCoords","Show Coords",     true);
    m_settings.defineFloat("r",         "Grid R",         114.f, 0.f, 255.f);
    m_settings.defineFloat("g",         "Grid G",         137.f, 0.f, 255.f);
    m_settings.defineFloat("b",         "Grid B",         218.f, 0.f, 255.f);
}

void ChunkMap::onRender(ImDrawList* dl) {
    auto* lp = getLocalPlayer();
    Vec3 pos = lp ? lp->getPosition() : Vec3{0,0,0};

    int radius = m_settings.getInt("radius");
    float cs   = m_settings.getFloat("size");
    float alpha= m_settings.getFloat("alpha");
    bool  sc   = m_settings.getBool("showCoords");
    ImU32 gridCol = IM_COL32((int)m_settings.getFloat("r"),
                              (int)m_settings.getFloat("g"),
                              (int)m_settings.getFloat("b"),
                              (int)(alpha * 200.f));
    ImU32 curChunk= IM_COL32((int)m_settings.getFloat("r"),
                              (int)m_settings.getFloat("g"),
                              (int)m_settings.getFloat("b"),
                              (int)(alpha * 60.f));

    // Center of the widget
    float wx = m_pos.x, wy = m_pos.y;
    float diam = cs * (radius * 2 + 1);

    // Background
    dl->AddRectFilled({ wx, wy }, { wx + diam, wy + diam }, IM_COL32(30,33,36,(int)(alpha*220)), 6.f);
    dl->AddRect({ wx, wy }, { wx + diam, wy + diam }, gridCol, 6.f, 0, 1.f);

    // Player chunk
    int pcx = (int)floorf(pos.x / 16.f);
    int pcz = (int)floorf(pos.z / 16.f);

    for (int dz = -radius; dz <= radius; dz++) {
        for (int dx = -radius; dx <= radius; dx++) {
            float cx = wx + (dx + radius) * cs;
            float cy = wy + (dz + radius) * cs;

            if (dx == 0 && dz == 0)
                dl->AddRectFilled({ cx, cy }, { cx + cs, cy + cs }, curChunk, 3.f);

            dl->AddRect({ cx, cy }, { cx + cs, cy + cs }, gridCol, 0.f, 0, 1.f);

            if (sc) {
                char buf[24];
                snprintf(buf, sizeof(buf), "%d,%d", pcx + dx, pcz + dz);
                ImVec2 tsz = ImGui::CalcTextSize(buf);
                float tx = cx + (cs - tsz.x) * 0.5f;
                float ty = cy + (cs - tsz.y) * 0.5f;
                dl->AddText(nullptr, 9.f, { tx, ty }, gridCol, buf);
            }
        }
    }

    // Player dot
    float offX = fmodf(pos.x, 16.f) / 16.f;
    float offZ = fmodf(pos.z, 16.f) / 16.f;
    if (offX < 0.f) offX += 1.f;
    if (offZ < 0.f) offZ += 1.f;
    float dotX = wx + radius * cs + offX * cs;
    float dotZ = wy + radius * cs + offZ * cs;
    dl->AddCircleFilled({ dotX, dotZ }, 4.f, IM_COL32(255,255,255,220));
    dl->AddCircle({ dotX, dotZ }, 4.f, IM_COL32(30,33,36,255), 0, 1.5f);
}
