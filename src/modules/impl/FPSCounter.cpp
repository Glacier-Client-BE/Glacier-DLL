#include "FPSCounter.h"
#include <IconsFontAwesome5.h>
#include <numeric>
#include <cstdio>

FPSCounter::FPSCounter()
    : ModuleBase("FPS Counter", "Smoothed frame-rate counter",
                 ICON_FA_TACHOMETER_ALT, ModuleCategory::HUD, 10.f, 10.f)
{
    m_settings.defineBool ("shadow",    "Text Shadow",   true);
    m_settings.defineBool ("showGraph", "Show Graph",   false);
    m_settings.defineFloat("fontSize",  "Font Size",    16.f, 8.f, 36.f);
    m_settings.defineInt  ("samples",   "Avg Samples",  60,   5,  240);
    m_settings.defineFloat("r",         "Color R",     100.f, 0.f, 255.f);
    m_settings.defineFloat("g",         "Color G",     200.f, 0.f, 255.f);
    m_settings.defineFloat("b",         "Color B",     255.f, 0.f, 255.f);
}

void FPSCounter::onEnable()  { m_last = Clock::now(); m_samples.clear(); m_fps = 0.f; }
void FPSCounter::onDisable() { m_samples.clear(); }

void FPSCounter::onRender(ImDrawList* dl) {
    auto  now = Clock::now();
    float dt  = std::chrono::duration<float>(now - m_last).count();
    m_last = now;
    const int max = m_settings.getInt("samples");
    if (dt > 0.f && dt < 1.f) { m_samples.push_back(dt); while ((int)m_samples.size() > max) m_samples.pop_front(); }
    if (!m_samples.empty()) {
        float avg = std::accumulate(m_samples.begin(), m_samples.end(), 0.f) / m_samples.size();
        m_fps = avg > 0.f ? 1.f / avg : 0.f;
    }
    char buf[32]; snprintf(buf, sizeof(buf), "FPS: %.0f", m_fps);
    float fs = m_settings.getFloat("fontSize");
    ImU32 col = IM_COL32((int)m_settings.getFloat("r"), (int)m_settings.getFloat("g"), (int)m_settings.getFloat("b"), 255);
    if (m_settings.getBool("shadow"))
        dl->AddText(nullptr, fs, {m_pos.x+1.f, m_pos.y+1.f}, IM_COL32(0,0,0,180), buf);
    dl->AddText(nullptr, fs, m_pos, col, buf);
}
