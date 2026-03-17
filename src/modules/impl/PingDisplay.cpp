#include "PingDisplay.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

PingDisplay::PingDisplay()
    : ModuleBase("Ping Display","Shows network latency",ICON_FA_WIFI,ModuleCategory::HUD,10.f,490.f)
{
    m_settings.defineFloat("fontSize","Font Size",14.f,8.f,28.f);
    m_settings.defineBool ("shadow",  "Shadow",    true);
    m_settings.defineInt  ("warnMs",  "Warn Above (ms)",150,50,500);
    m_settings.defineInt  ("badMs",   "Bad Above (ms)", 300,100,1000);
    m_settings.defineBool ("showBar", "Show Quality Bar",true);
    m_settings.defineFloat("barWidth","Bar Width",80.f,30.f,160.f);
}

void PingDisplay::onRenderImGui() {
    int ping = 0;
    auto* lp = getLocalPlayer();
    if (lp) ping = lp->getNetworkPing();
    // If SDK returns 0 (not in game) show a dash
    bool inGame = (lp != nullptr);

    float fs   = m_settings.getFloat("fontSize");
    bool  shad = m_settings.getBool("shadow");
    bool  bar  = m_settings.getBool("showBar");
    float bw   = m_settings.getFloat("barWidth");
    int   warnMs = m_settings.getInt("warnMs");
    int   badMs  = m_settings.getInt("badMs");

    ImU32 col = !inGame                  ? IM_COL32(100,100,100,180)
              : ping < warnMs            ? IM_COL32(72,199,142,255)
              : ping < badMs             ? IM_COL32(255,189,51,255)
                                         : IM_COL32(237,70,70,255);

    char buf[32];
    if (inGame) snprintf(buf,sizeof(buf), ICON_FA_WIFI "  %d ms", ping);
    else        snprintf(buf,sizeof(buf), ICON_FA_WIFI "  -- ms");

    float winH = fs + 4.f + (bar ? 10.f : 0.f);
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::SetNextWindowSize({bw, winH});
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{2,2});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    if (ImGui::Begin("##ping",nullptr,f)) {
        if (ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp = ImGui::GetWindowPos();
        if (shad) dl->AddText(nullptr,fs,{bp.x+1,bp.y+1},IM_COL32(0,0,0,160),buf);
        dl->AddText(nullptr,fs,bp,col,buf);

        if (bar && inGame) {
            float oy = fs + 4.f;
            float ratio = (float)ping / (float)badMs;
            if (ratio > 1.f) ratio = 1.f;
            // Bar fills right for bad ping, empty = good
            ImU32 barCol = ratio < 0.5f ? IM_COL32(72,199,142,200)
                         : ratio < 0.75f? IM_COL32(255,189,51,200)
                                        : IM_COL32(237,70,70,200);
            dl->AddRectFilled({bp.x,bp.y+oy},{bp.x+bw,bp.y+oy+6},IM_COL32(35,39,42,200),3);
            dl->AddRectFilled({bp.x,bp.y+oy},{bp.x+bw*ratio,bp.y+oy+6},barCol,3);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
