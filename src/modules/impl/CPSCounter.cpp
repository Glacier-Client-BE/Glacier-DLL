#include "CPSCounter.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>

CPSCounter::CPSCounter()
    : ModuleBase("CPS Counter", "Left/right clicks per second with peak tracking and bar",
                 "cpscounter", ModuleCategory::HUD, 10.f, 60.f)
{
    m_settings.defineBool ("showRight", "Show RMB",     true);
    m_settings.defineBool ("showPeak",  "Show Peak",    true);
    m_settings.defineBool ("showBar",   "CPS Bar",      true);
    m_settings.defineInt  ("maxCPS",    "Bar Max CPS",  20, 5, 50);
    m_settings.defineFloat("scale",     "Scale",        1.f, 0.5f, 2.f);
}

void CPSCounter::onEnable() { m_L.clear(); m_R.clear(); m_pL=m_pR=false; m_peakL=m_peakR=0; }

void CPSCounter::onRenderImGui() {
    auto now = Clock::now();
    bool cL = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool cR = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if (cL && !m_pL) m_L.push_back(now);
    if (cR && !m_pR) m_R.push_back(now);
    m_pL = cL; m_pR = cR;
    auto cut = now - std::chrono::seconds(1);
    while (!m_L.empty() && m_L.front() < cut) m_L.pop_front();
    while (!m_R.empty() && m_R.front() < cut) m_R.pop_front();
    int lc = (int)m_L.size(), rc = (int)m_R.size();
    if (lc > m_peakL) m_peakL = lc;
    if (rc > m_peakR) m_peakR = rc;

    float sc   = m_settings.getFloat("scale");
    bool  sR   = m_settings.getBool("showRight");
    bool  sPk  = m_settings.getBool("showPeak");
    bool  sBar = m_settings.getBool("showBar");
    int   mx   = m_settings.getInt("maxCPS");

    float rowH = HUDStyle::FONT_BIG * sc
        + (sBar ? 10.f * sc : 0.f)
        + (sPk  ? HUDStyle::FONT_SMALL * sc + 3.f : 0.f)
        + 5.f;
    int   rows = 1 + (sR ? 1 : 0);
    float panW = 100.f * sc;
    float panH = rowH * rows + HUDStyle::PAD_Y * 2;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ panW, panH });
    HUDStyle::push();
    if (ImGui::Begin("##cps", nullptr, HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp = ImGui::GetWindowPos();
        float  oy = HUDStyle::PAD_Y;
        float  bw = panW - HUDStyle::PAD_X * 2;

        auto drawRow = [&](int cps, int peak, ImU32 col, const char* lbl) {
            char buf[32]; snprintf(buf, sizeof(buf), "%d", cps);
            float bx = bp.x + HUDStyle::PAD_X;
            // Big number
            HUDStyle::text(dl, HUDStyle::FONT_BIG * sc, {bx, bp.y+oy}, col, buf);
            // Label
            ImVec2 numSz = ImGui::CalcTextSize(buf);
            float scale_ = HUDStyle::FONT_BIG * sc / ImGui::GetFontSize();
            HUDStyle::text(dl, HUDStyle::FONT_SMALL * sc,
                {bx + numSz.x * scale_ + 4.f, bp.y + oy + HUDStyle::FONT_BIG * sc * 0.6f},
                HUDStyle::GREY, lbl, false);
            oy += HUDStyle::FONT_BIG * sc + 2.f;
            if (sBar) {
                float ratio = mx > 0 ? (float)cps / mx : 0.f;
                HUDStyle::bar(dl, bx, bp.y+oy, bw, 6.f*sc, ratio, col, 3.f);
                oy += 10.f * sc;
            }
            if (sPk) {
                char pb[16]; snprintf(pb, sizeof(pb), "Peak %d", peak);
                HUDStyle::text(dl, HUDStyle::FONT_SMALL * sc, {bx, bp.y+oy},
                    HUDStyle::YELLOW, pb, false);
                oy += HUDStyle::FONT_SMALL * sc + 3.f;
            }
            oy += 5.f;
        };

        drawRow(lc, m_peakL, HUDStyle::ACCENT, "LMB");
        if (sR) drawRow(rc, m_peakR, HUDStyle::GREY, "RMB");
    }
    ImGui::End();
    HUDStyle::pop();
}
