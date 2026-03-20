#include "PingDisplay.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

PingDisplay::PingDisplay()
    : ModuleBase("Ping", "Network latency with signal bars and history graph",
                 "pingdisplay", ModuleCategory::HUD, 10.f, 490.f)
{
    m_settings.defineInt  ("goodMs",  "Good  ≤",       80,  10, 300);
    m_settings.defineInt  ("warnMs",  "Warn  ≤",      150,  50, 500);
    m_settings.defineInt  ("badMs",   "Bad   >",       300, 100,1000);
    m_settings.defineBool ("showBars","Signal Bars",    true);
    m_settings.defineBool ("showGraph","History Graph",  false);
    m_settings.defineFloat("graphW",  "Graph Width",   80.f, 40.f,200.f);
    m_settings.defineFloat("graphH",  "Graph Height",  20.f,  8.f, 50.f);
    m_settings.defineFloat("scale",   "Scale",          1.f,  0.5f,  2.f);
}

void PingDisplay::onEnable()  { m_history.clear(); m_accumDt = 0.f; }
void PingDisplay::onDisable() { m_history.clear(); }

void PingDisplay::onRenderImGui() {
    auto* lp   = getLocalPlayer();
    int   ping = lp ? lp->getNetworkPing() : -1;
    bool  inGame = lp && ping >= 0;

    m_accumDt += ImGui::GetIO().DeltaTime;
    if (m_accumDt >= 0.1f && inGame) {
        m_accumDt = 0.f;
        int ms = 80;
        m_history.push_back((float)ping);
        while ((int)m_history.size() > ms) m_history.pop_front();
    } else if (m_accumDt >= 0.1f) { m_accumDt = 0.f; }

    float sc  = m_settings.getFloat("scale");
    int   g   = m_settings.getInt("goodMs");
    int   w   = m_settings.getInt("warnMs");
    int   b   = m_settings.getInt("badMs");
    bool  sB  = m_settings.getBool("showBars");
    bool  sG  = m_settings.getBool("showGraph");
    float gw  = m_settings.getFloat("graphW") * sc;
    float gh  = m_settings.getFloat("graphH") * sc;

    ImU32 col = !inGame ? HUDStyle::GREY
              : ping<=g ? HUDStyle::GREEN
              : ping<=w ? HUDStyle::YELLOW
                        : HUDStyle::RED;

    int bars = !inGame ? 0 : ping<=g ? 4 : ping<=w ? 3 : ping<=b ? 2 : 1;

    char buf[32];
    if (inGame) snprintf(buf,sizeof(buf),"%d ms",ping);
    else        snprintf(buf,sizeof(buf),"-- ms");

    float panW = std::max(gw+HUDStyle::PAD_X*2, 80.f*sc);
    float panH = HUDStyle::FONT_BIG*sc
        + (sB ? 14.f*sc : 0.f)
        + (sG ? gh+6.f : 0.f)
        + HUDStyle::PAD_Y*2;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ panW, panH });
    HUDStyle::push();
    if (ImGui::Begin("##ping", nullptr, HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp = ImGui::GetWindowPos();
        float  oy = HUDStyle::PAD_Y;
        float  bx = bp.x + HUDStyle::PAD_X;
        float  bw = panW - HUDStyle::PAD_X*2;

        HUDStyle::text(dl, HUDStyle::FONT_BIG*sc, {bx,bp.y+oy}, col, buf);
        oy += HUDStyle::FONT_BIG*sc + 4.f;

        if (sB && inGame) {
            float barW = (bw - 3.f*3.f*sc) / 4.f;
            for (int i=0;i<4;i++) {
                float bh2 = (4.f+i*3.f)*sc;
                float bbx = bx + i*(barW+3.f*sc);
                float bby = bp.y+oy+(12.f*sc-bh2);
                ImU32 bc  = i < bars ? col : IM_COL32(50,52,58,180);
                dl->AddRectFilled({bbx,bby},{bbx+barW,bby+bh2}, bc, 2.f);
            }
            oy += 14.f*sc;
        }

        if (sG && (int)m_history.size()>1) {
            float maxV = *std::max_element(m_history.begin(),m_history.end());
            float minV = *std::min_element(m_history.begin(),m_history.end());
            if (maxV-minV<10.f) maxV=minV+10.f;
            dl->AddRectFilled({bx,bp.y+oy},{bx+gw,bp.y+oy+gh},HUDStyle::BAR_BG,3.f);
            int n=(int)m_history.size(); float sw=gw/(n-1);
            for (int i=1;i<n;i++) {
                float x1=bx+(i-1)*sw,x2=bx+i*sw;
                float y1=bp.y+oy+gh-((m_history[i-1]-minV)/(maxV-minV))*gh;
                float y2=bp.y+oy+gh-((m_history[i  ]-minV)/(maxV-minV))*gh;
                y1=y1<bp.y+oy?bp.y+oy:y1; y2=y2<bp.y+oy?bp.y+oy:y2;
                dl->AddLine({x1,y1},{x2,y2},col,1.2f);
            }
        }
    }
    ImGui::End();
    HUDStyle::pop();
}
