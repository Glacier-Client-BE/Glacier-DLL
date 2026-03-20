#include "FPSCounter.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>
#include <numeric>

FPSCounter::FPSCounter()
    : ModuleBase("FPS Counter", "Smoothed frame-rate with EMA, color ramp and optional graph",
                 "fpscounter", ModuleCategory::HUD, 10.f, 10.f)
{
    m_settings.defineFloat("emaAlpha",    "EMA Smoothing",     0.05f, 0.01f, 0.5f);
    m_settings.defineBool ("colorRamp",   "Color by FPS",       true);
    m_settings.defineInt  ("goodFPS",     "Good FPS",           60,   20,  240);
    m_settings.defineInt  ("warnFPS",     "Warn FPS",           30,    5,  120);
    m_settings.defineBool ("showGraph",   "Show Graph",         false);
    m_settings.defineFloat("graphW",      "Graph Width",        90.f, 40.f,200.f);
    m_settings.defineFloat("graphH",      "Graph Height",       24.f,  8.f, 60.f);
    m_settings.defineInt  ("graphSamples","Graph Samples",       80,   20, 200);
    m_settings.defineFloat("r",           "Custom R",          114.f,  0.f,255.f);
    m_settings.defineFloat("g",           "Custom G",          200.f,  0.f,255.f);
    m_settings.defineFloat("b",           "Custom B",          255.f,  0.f,255.f);
    m_settings.defineFloat("scale",       "Scale",               1.f, 0.5f,  2.f);
}

void FPSCounter::onEnable()  { m_last = Clock::now(); m_fps = 0.f; m_slow = 0.f; m_graph.clear(); }
void FPSCounter::onDisable() { m_graph.clear(); }

void FPSCounter::onRender(ImDrawList* dl) {
    auto  now = Clock::now();
    float dt  = std::chrono::duration<float>(now - m_last).count();
    m_last = now;

    if (dt > 0.f && dt < 1.f) {
        float inst = 1.f / dt;
        float a    = m_settings.getFloat("emaAlpha");
        m_fps  = m_fps  < 1.f ? inst : a * inst + (1.f - a) * m_fps;
        m_slow = m_slow < 1.f ? inst : 0.01f * inst + 0.99f * m_slow;
        int ms = m_settings.getInt("graphSamples");
        m_graph.push_back(m_fps);
        while ((int)m_graph.size() > ms) m_graph.pop_front();
    }

    float sc = m_settings.getFloat("scale");
    float bw = HUDStyle::FONT_BIG * sc;
    bool  gr = m_settings.getBool("showGraph");
    float gw = m_settings.getFloat("graphW") * sc;
    float gh = m_settings.getFloat("graphH") * sc;

    ImU32 col;
    if (m_settings.getBool("colorRamp")) {
        int fps = (int)m_fps;
        col = fps >= m_settings.getInt("goodFPS") ? HUDStyle::GREEN
            : fps >= m_settings.getInt("warnFPS") ? HUDStyle::YELLOW
                                                  : HUDStyle::RED;
    } else {
        col = IM_COL32((int)m_settings.getFloat("r"),
                       (int)m_settings.getFloat("g"),
                       (int)m_settings.getFloat("b"), 255);
    }

    char buf[32]; snprintf(buf, sizeof(buf), "%.0f FPS", m_fps);
    float panW = std::max(gw + HUDStyle::PAD_X * 2, 80.f * sc);
    float panH = HUDStyle::FONT_BIG * sc + (gr ? gh + 6.f : 0.f) + HUDStyle::PAD_Y * 2;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ panW, panH });
    HUDStyle::push();
    if (ImGui::Begin("##fps", nullptr, HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImVec2 bp = ImGui::GetWindowPos();
        float  oy = HUDStyle::PAD_Y;
        HUDStyle::text(dl, HUDStyle::FONT_BIG * sc, { bp.x + HUDStyle::PAD_X, bp.y + oy }, col, buf);
        oy += HUDStyle::FONT_BIG * sc + 4.f;
        if (gr && (int)m_graph.size() > 1) {
            float maxV = *std::max_element(m_graph.begin(), m_graph.end());
            if (maxV < 1.f) maxV = 1.f;
            float gx = bp.x + HUDStyle::PAD_X;
            float gy = bp.y + oy;
            dl->AddRectFilled({gx,gy},{gx+gw,gy+gh}, HUDStyle::BAR_BG, 3.f);
            int n = (int)m_graph.size();
            float sw = gw / (n - 1);
            for (int i = 1; i < n; i++) {
                float x1=gx+(i-1)*sw, x2=gx+i*sw;
                float y1=gy+gh-(m_graph[i-1]/maxV)*gh;
                float y2=gy+gh-(m_graph[i  ]/maxV)*gh;
                dl->AddLine({x1,y1},{x2,y2}, col, 1.3f);
            }
            float refY = gy+gh-(m_slow/maxV)*gh;
            refY = refY < gy ? gy : refY > gy+gh ? gy+gh : refY;
            dl->AddLine({gx,refY},{gx+gw,refY}, IM_COL32(255,255,255,25), 1.f);
        }
    }
    ImGui::End();
    HUDStyle::pop();
}
