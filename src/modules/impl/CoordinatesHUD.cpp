#include "CoordinatesHUD.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>
#include <cmath>

CoordinatesHUD::CoordinatesHUD()
    : ModuleBase("Coordinates","Shows X / Y / Z and facing direction",
                 ICON_FA_MAP_MARKER_ALT, ModuleCategory::HUD, 10.f, 220.f)
{
    m_settings.defineBool ("showXYZ",    "Show XYZ",         true);
    m_settings.defineBool ("showFacing", "Show Facing",       true);
    m_settings.defineBool ("showChunk",  "Show Chunk Coords", true);
    m_settings.defineBool ("showDim",    "Show Dimension",    true);
    m_settings.defineBool ("showNether", "Show Nether Coords",true);
    m_settings.defineBool ("shadow",     "Text Shadow",       true);
    m_settings.defineBool ("bgBox",      "Background Box",    true);
    m_settings.defineFloat("fontSize",   "Font Size",        13.f, 8.f, 28.f);
    m_settings.defineFloat("bgAlpha",    "BG Alpha",         0.7f, 0.f, 1.f);
    m_settings.defineInt  ("precision",  "Decimal Places",    1,   0,   3);
}

void CoordinatesHUD::onRenderImGui() {
    // Pull position and rotation from SDK
    float px = 128.5f, py = 64.0f, pz = -256.3f, yaw = 45.f;
    int   dim = 0;

    auto* lp = getLocalPlayer();
    if (lp) {
        Vec3 pos = lp->getPosition();
        px  = pos.x; py  = pos.y; pz  = pos.z;
        yaw = lp->getYaw();
        dim = lp->getDimension();
    }

    bool showXYZ    = m_settings.getBool("showXYZ");
    bool showFacing = m_settings.getBool("showFacing");
    bool showChunk  = m_settings.getBool("showChunk");
    bool showDim    = m_settings.getBool("showDim");
    bool showNether = m_settings.getBool("showNether");
    bool shadow     = m_settings.getBool("shadow");
    bool bgBox      = m_settings.getBool("bgBox");
    float fs   = m_settings.getFloat("fontSize");
    float bgA  = m_settings.getFloat("bgAlpha");
    int   prec = m_settings.getInt("precision");

    // Normalize yaw to 0-360
    while (yaw < 0.f)    yaw += 360.f;
    while (yaw >= 360.f) yaw -= 360.f;

    const char* dirs[8] = {"S","SW","W","NW","N","NE","E","SE"};
    int dirIdx = (int)((yaw + 22.5f) / 45.f) & 7;

    // Chunk coords
    int cx = (int)floorf(px / 16.f);
    int cz = (int)floorf(pz / 16.f);

    // Nether/overworld conversion
    float netherX = px / 8.f, netherZ = pz / 8.f;

    static const char* dimName[] = {"Overworld","Nether","The End"};
    const char* dimStr = (dim >= 0 && dim <= 2) ? dimName[dim] : "Unknown";

    char xbuf[40], ybuf[40], zbuf[40], fbuf[40], cbuf[56], dbuf[32], nbuf[64];
    snprintf(xbuf, sizeof(xbuf), "X: %.*f", prec, (double)px);
    snprintf(ybuf, sizeof(ybuf), "Y: %.*f", prec, (double)py);
    snprintf(zbuf, sizeof(zbuf), "Z: %.*f", prec, (double)pz);
    snprintf(fbuf, sizeof(fbuf), "Facing: %s (%.0f°)", dirs[dirIdx], (double)yaw);
    snprintf(cbuf, sizeof(cbuf), "Chunk: %d, %d", cx, cz);
    snprintf(dbuf, sizeof(dbuf), "Dim: %s", dimStr);
    if (dim == 0)
        snprintf(nbuf, sizeof(nbuf), "Nether: %.1f, %.1f", (double)netherX, (double)netherZ);
    else if (dim == 1)
        snprintf(nbuf, sizeof(nbuf), "Overworld: %.1f, %.1f", (double)(px*8.f), (double)(pz*8.f));
    else
        nbuf[0] = '\0';

    float lineH = fs + 3.f;
    float lines = (showXYZ ? 3.f : 0.f) + (showFacing ? 1.f : 0.f)
                + (showChunk ? 1.f : 0.f)
                + (showDim ? 1.f : 0.f)
                + (showNether && nbuf[0] ? 1.f : 0.f);
    float boxW = 155.f, boxH = lines * lineH + 10.f;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({boxW, boxH});
    ImGui::SetNextWindowBgAlpha(bgBox ? bgA : 0.f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.137f,0.153f,0.165f, bgBox ? bgA : 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.447f,0.537f,0.855f, bgBox ? 0.4f : 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  {6.f, 4.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  6.f);

    if (ImGui::Begin("##coords", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta; m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos(); p.x += 6.f; p.y += 4.f;
        float oy = 0.f;

        auto draw = [&](const char* txt, ImU32 col) {
            if (shadow) dl->AddText(nullptr, fs, {p.x+1, p.y+oy+1}, IM_COL32(0,0,0,160), txt);
            dl->AddText(nullptr, fs, {p.x, p.y+oy}, col, txt);
            oy += lineH;
        };

        if (showXYZ) {
            draw(xbuf, IM_COL32(255,100,100,255));
            draw(ybuf, IM_COL32(100,220,100,255));
            draw(zbuf, IM_COL32(100,160,255,255));
        }
        if (showFacing)           draw(fbuf, IM_COL32(255,220,100,255));
        if (showChunk)            draw(cbuf, IM_COL32(180,180,180,200));
        if (showDim)              draw(dbuf, IM_COL32(200,150,255,200));
        if (showNether && nbuf[0])draw(nbuf, IM_COL32(255,130, 70,200));
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
