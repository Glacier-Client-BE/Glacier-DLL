#include "Crosshair.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cmath>
#include <chrono>

Crosshair::Crosshair()
    : ModuleBase("Crosshair", "Fully customizable crosshair with multiple styles and dynamic gap on attack",
                 "crosshair", ModuleCategory::Visual)
{
    // Style: 0=Cross, 1=Dot only, 2=Circle, 3=T-shape
    m_settings.defineInt  ("style",      "Style (0-3)",      0,     0,    3);
    m_settings.defineFloat("size",       "Size",             8.f,   1.f, 40.f);
    m_settings.defineFloat("thick",      "Thickness",        1.5f,  0.5f, 6.f);
    m_settings.defineFloat("gap",        "Center Gap",       3.f,   0.f, 20.f);
    m_settings.defineFloat("r",          "Color R",        255.f,   0.f,255.f);
    m_settings.defineFloat("g",          "Color G",        255.f,   0.f,255.f);
    m_settings.defineFloat("b",          "Color B",        255.f,   0.f,255.f);
    m_settings.defineFloat("alpha",      "Alpha",            0.9f,  0.f,  1.f);
    m_settings.defineBool ("dot",        "Center Dot",       false);
    m_settings.defineBool ("outline",    "Outline",          true);
    m_settings.defineFloat("outAlpha",   "Outline Alpha",    0.55f, 0.f,  1.f);
    m_settings.defineBool ("dynamicGap", "Dynamic Gap (LMB)",true);
    m_settings.defineFloat("atkGap",     "Attack Gap",       6.f,   0.f, 30.f);
    m_settings.defineFloat("atkFade",    "Attack Fade (ms)", 120.f, 20.f,400.f);
    m_settings.defineFloat("circleR",    "Circle Radius",   10.f,   3.f, 40.f);
    m_settings.defineInt  ("circleSegs", "Circle Segments", 32,     8,   64);
    m_settings.defineBool ("hideInMenu", "Hide In Menu",     true);
}

void Crosshair::onEnable()  { m_prevLMB = false; m_atkFlash = 0.f; }
void Crosshair::onDisable() { m_atkFlash = 0.f; }

void Crosshair::onRender(ImDrawList* dl) {
    ImGuiIO& io = ImGui::GetIO();

    // Optionally hide when mod menu is open (forward-declared via ModMenu singleton check is
    // avoided here — just skip when no display size available)
    if (io.DisplaySize.x < 1.f) return;

    float cx = io.DisplaySize.x * 0.5f;
    float cy = io.DisplaySize.y * 0.5f;

    // Dynamic gap on LMB press
    bool curLMB = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (curLMB && !m_prevLMB) m_atkFlash = 1.f;
    m_prevLMB = curLMB;

    float fadeMs = m_settings.getFloat("atkFade");
    m_atkFlash -= (io.DeltaTime * 1000.f) / fadeMs;
    if (m_atkFlash < 0.f) m_atkFlash = 0.f;

    float gap = m_settings.getFloat("gap");
    if (m_settings.getBool("dynamicGap") && m_atkFlash > 0.f)
        gap = gap + (m_settings.getFloat("atkGap") - gap) * m_atkFlash;

    float sz    = m_settings.getFloat("size");
    float th    = m_settings.getFloat("thick");
    ImU32 col   = IM_COL32((int)m_settings.getFloat("r"),
                            (int)m_settings.getFloat("g"),
                            (int)m_settings.getFloat("b"),
                            (int)(m_settings.getFloat("alpha") * 255.f));
    ImU32 outCol = IM_COL32(0, 0, 0, (int)(m_settings.getFloat("outAlpha") * 255.f));
    bool  outline = m_settings.getBool("outline");
    float thOut   = th + 2.f;

    int style = m_settings.getInt("style");

    if (style == 2) {
        // Circle style
        float r = m_settings.getFloat("circleR") + m_atkFlash * (gap * 0.5f);
        int   segs = m_settings.getInt("circleSegs");
        if (outline) dl->AddCircle({cx, cy}, r + 1.f, outCol, segs, thOut);
        dl->AddCircle({cx, cy}, r, col, segs, th);
        if (m_settings.getBool("dot")) {
            if (outline) dl->AddCircleFilled({cx, cy}, th + 1.5f, outCol);
            dl->AddCircleFilled({cx, cy}, th, col);
        }
        return;
    }

    // Helper: draw a line with optional outline
    auto line = [&](ImVec2 a, ImVec2 b) {
        if (outline) dl->AddLine(a, b, outCol, thOut);
        dl->AddLine(a, b, col, th);
    };

    if (style == 1) {
        // Dot only
        if (outline) dl->AddCircleFilled({cx, cy}, th + 2.f, outCol);
        dl->AddCircleFilled({cx, cy}, th + 0.5f, col);
        return;
    }

    if (style == 3) {
        // T-shape: left, right, bottom (no top arm)
        line({cx - sz, cy}, {cx - gap, cy});
        line({cx + gap, cy}, {cx + sz, cy});
        line({cx, cy + gap}, {cx, cy + sz});
    } else {
        // Standard cross (style == 0)
        line({cx - sz, cy}, {cx - gap, cy});
        line({cx + gap, cy}, {cx + sz, cy});
        line({cx, cy - sz}, {cx, cy - gap});
        line({cx, cy + gap}, {cx, cy + sz});
    }

    if (m_settings.getBool("dot")) {
        if (outline) dl->AddCircleFilled({cx, cy}, th + 1.5f, outCol);
        dl->AddCircleFilled({cx, cy}, th, col);
    }
}
