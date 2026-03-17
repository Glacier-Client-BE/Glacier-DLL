#include "ReachDisplay.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

ReachDisplay::ReachDisplay()
    : ModuleBase("Reach Display","Shows distance to last hit target",
                 ICON_FA_RULER, ModuleCategory::Combat, 10.f, 540.f)
{
    m_settings.defineFloat("fontSize",  "Font Size",      13.f, 8.f, 28.f);
    m_settings.defineBool ("shadow",    "Shadow",          true);
    m_settings.defineFloat("warnAt",    "Warn Color Above",3.0f, 1.f, 6.f);
    m_settings.defineBool ("showIcon",  "Show Icon",       true);
    m_settings.defineBool ("showNoData","Show Before Hit", true);
}

void ReachDisplay::onRenderImGui() {
    auto& rt = ReachTracker::get();

    if (!rt.hasData && !m_settings.getBool("showNoData")) return;

    float reach = rt.hasData ? rt.lastReach : 0.f;
    float warnAt = m_settings.getFloat("warnAt");
    float fs   = m_settings.getFloat("fontSize");
    bool  shad = m_settings.getBool("shadow");

    // Green when reach is normal (≤warnAt), red when suspiciously high
    ImU32 col = (reach <= warnAt)
        ? IM_COL32(72, 199, 142, 255)
        : IM_COL32(237, 70, 70, 255);

    char buf[48];
    if (rt.hasData)
        snprintf(buf, sizeof(buf), "%s%.2f blocks",
                 m_settings.getBool("showIcon") ? ICON_FA_RULER " " : "", reach);
    else
        snprintf(buf, sizeof(buf), "%sNo hits yet",
                 m_settings.getBool("showIcon") ? ICON_FA_RULER " " : "");

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::SetNextWindowSize({0,0});
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   {2,2});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    if (ImGui::Begin("##reach", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta; m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp = ImGui::GetWindowPos();
        if (shad) dl->AddText(nullptr, fs, {bp.x+1, bp.y+1}, IM_COL32(0,0,0,160), buf);
        dl->AddText(nullptr, fs, bp, col, buf);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
