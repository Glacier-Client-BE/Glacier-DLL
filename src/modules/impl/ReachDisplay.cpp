// ─ ReachDisplay ──────────────────────────────────────────────────────────────
#include "ReachDisplay.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <numeric>
#include <algorithm>

ReachDisplay::ReachDisplay()
    : ModuleBase("Reach", "Last attack reach with rolling average and threshold color",
                 "reachdisplay", ModuleCategory::HUD, 10.f, 540.f)
{
    m_settings.defineFloat("warnAt",  "Warn Above",  3.0f, 1.f, 6.f);
    m_settings.defineBool ("showAvg", "Show Avg",    true);
    m_settings.defineBool ("showMax", "Show Max",    false);
    m_settings.defineBool ("noData",  "Show Before Hit", true);
    m_settings.defineInt  ("samples", "Avg Samples", 5,    2, 20);
    m_settings.defineFloat("scale",   "Scale",       1.f,  0.5f, 2.f);
}
void ReachDisplay::onEnable()  { m_samples.clear(); }
void ReachDisplay::onDisable() { m_samples.clear(); }
void ReachDisplay::onTick() {
    auto& rt = ReachTracker::get();
    if (rt.hasData && rt.lastReach != m_lastRec) {
        m_lastRec = rt.lastReach;
        int ms = m_settings.getInt("samples");
        m_samples.push_back(rt.lastReach);
        while ((int)m_samples.size() > ms) m_samples.pop_front();
    }
}
void ReachDisplay::onRenderImGui() {
    auto& rt = ReachTracker::get();
    if (!rt.hasData && !m_settings.getBool("noData")) return;

    float reach  = rt.hasData ? rt.lastReach : 0.f;
    float warn   = m_settings.getFloat("warnAt");
    float sc     = m_settings.getFloat("scale");
    ImU32 col    = !rt.hasData ? HUDStyle::GREY
                 : reach<=warn ? HUDStyle::GREEN : HUDStyle::RED;

    float avg=0.f, maxR=0.f;
    if (!m_samples.empty()) {
        avg  = std::accumulate(m_samples.begin(),m_samples.end(),0.f)/(float)m_samples.size();
        maxR = *std::max_element(m_samples.begin(),m_samples.end());
    }

    bool sA = m_settings.getBool("showAvg") && !m_samples.empty();
    bool sM = m_settings.getBool("showMax") && !m_samples.empty();
    float fs  = HUDStyle::FONT_BIG * sc;
    float fss = HUDStyle::FONT_SMALL * sc;
    float lh  = fss + 3.f;
    float panW = 130.f * sc;
    float panH = fs + (sA?lh:0.f) + (sM?lh:0.f) + HUDStyle::PAD_Y*2;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ panW, panH });
    HUDStyle::push();
    if (ImGui::Begin("##reach", nullptr, HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp = ImGui::GetWindowPos();
        float bx = bp.x+HUDStyle::PAD_X, oy = HUDStyle::PAD_Y;
        char buf[48];
        if (rt.hasData) snprintf(buf,sizeof(buf),"%.3f blk",reach);
        else            snprintf(buf,sizeof(buf),"No hits");
        HUDStyle::text(dl,fs,{bx,bp.y+oy},col,buf);
        oy += fs + 2.f;
        if (sA) { snprintf(buf,sizeof(buf),"Avg %.3f",avg);
            HUDStyle::text(dl,fss,{bx,bp.y+oy},HUDStyle::GREY,buf,false); oy+=lh; }
        if (sM) { snprintf(buf,sizeof(buf),"Max %.3f",maxR);
            ImU32 mc=maxR>warn?HUDStyle::RED:HUDStyle::YELLOW;
            HUDStyle::text(dl,fss,{bx,bp.y+oy},mc,buf,false); }
    }
    ImGui::End();
    HUDStyle::pop();
}
