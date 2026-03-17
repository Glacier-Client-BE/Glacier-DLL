#include "SpeedHUD.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <numeric>

SpeedHUD::SpeedHUD()
    : ModuleBase("Speed HUD","Displays movement speed in Blocks Per Second",
                 ICON_FA_RUNNING, ModuleCategory::HUD, 10.f, 290.f)
{
    m_settings.defineBool ("showMax",     "Show Max Speed",   true);
    m_settings.defineBool ("showGraph",   "Show Graph",       true);
    m_settings.defineBool ("shadow",      "Shadow",           true);
    m_settings.defineFloat("fontSize",    "Font Size",       14.f, 8.f, 28.f);
    m_settings.defineFloat("graphW",      "Graph Width",     80.f, 40.f, 160.f);
    m_settings.defineFloat("graphH",      "Graph Height",    30.f, 15.f, 60.f);
    m_settings.defineInt  ("graphSamples","Graph Samples",   40,  10, 100);
    m_settings.defineBool ("resetMax",    "Reset Max on Enable", true);
    m_lastTime = std::chrono::high_resolution_clock::now();
}

void SpeedHUD::onEnable() {
    if (m_settings.getBool("resetMax")) m_maxBps = 0.f;
    m_history.clear();
    m_lastTime = std::chrono::high_resolution_clock::now();
    // Seed position from SDK if available
    auto* lp = getLocalPlayer();
    if (lp) {
        Vec3 p = lp->getPosition();
        m_lastX = p.x; m_lastZ = p.z;
    }
}

void SpeedHUD::onRenderImGui() {
    auto  now = std::chrono::high_resolution_clock::now();
    float dt  = std::chrono::duration<float>(now - m_lastTime).count();
    m_lastTime = now;

    // Pull real position from SDK; fall back to stub if not in-game
    auto* lp = getLocalPlayer();
    float curX = m_lastX, curZ = m_lastZ;
    if (lp) {
        Vec3 pos = lp->getPosition();
        curX = pos.x; curZ = pos.z;
    } else {
        // Stub: simulate slow movement for preview
        curX = m_lastX + 0.08f * dt;
        curZ = m_lastZ + 0.04f * dt;
    }

    if (dt > 0.f && dt < 1.f) {
        float dx = curX - m_lastX, dz = curZ - m_lastZ;
        m_bps = sqrtf(dx*dx + dz*dz) / dt;
        if (m_bps > m_maxBps) m_maxBps = m_bps;
        int maxS = m_settings.getInt("graphSamples");
        m_history.push_back(m_bps);
        while ((int)m_history.size() > maxS) m_history.pop_front();
    }
    m_lastX = curX; m_lastZ = curZ;

    float fs      = m_settings.getFloat("fontSize");
    float gw      = m_settings.getFloat("graphW");
    float gh      = m_settings.getFloat("graphH");
    bool  showMax = m_settings.getBool("showMax");
    bool  showG   = m_settings.getBool("showGraph");
    bool  shad    = m_settings.getBool("shadow");

    float boxH = fs + 4.f + (showMax ? fs+2.f : 0.f) + (showG ? gh+6.f : 0.f);
    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({gw, boxH});
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   {0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    if (ImGui::Begin("##speed", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta; m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp = ImGui::GetWindowPos();
        float oy = 0.f;

        char buf[32];
        snprintf(buf, sizeof(buf), "BPS: %.2f", m_bps);
        if (shad) dl->AddText(nullptr, fs, {bp.x+1, bp.y+oy+1}, IM_COL32(0,0,0,160), buf);
        dl->AddText(nullptr, fs, {bp.x, bp.y+oy}, IM_COL32(72,199,142,255), buf);
        oy += fs + 2.f;

        if (showMax) {
            snprintf(buf, sizeof(buf), "Max: %.2f", m_maxBps);
            if (shad) dl->AddText(nullptr, fs*.8f, {bp.x+1, bp.y+oy+1}, IM_COL32(0,0,0,120), buf);
            dl->AddText(nullptr, fs*.8f, {bp.x, bp.y+oy}, IM_COL32(153,170,181,200), buf);
            oy += fs*.8f + 2.f;
        }

        if (showG && m_history.size() > 1) {
            float maxV = *std::max_element(m_history.begin(), m_history.end());
            if (maxV < 0.01f) maxV = 0.01f;
            dl->AddRectFilled({bp.x, bp.y+oy}, {bp.x+gw, bp.y+oy+gh}, IM_COL32(35,39,42,180), 3);
            int   n  = (int)m_history.size();
            float sw = gw / (n-1);
            for (int i = 1; i < n; i++) {
                float x1 = bp.x+(i-1)*sw, x2 = bp.x+i*sw;
                float y1 = bp.y+oy+gh - (m_history[i-1]/maxV)*gh;
                float y2 = bp.y+oy+gh - (m_history[i]  /maxV)*gh;
                dl->AddLine({x1,y1},{x2,y2}, IM_COL32(72,199,142,200), 1.5f);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
