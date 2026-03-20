#include "HealthIndicator.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

HealthIndicator::HealthIndicator()
    : ModuleBase("Health Indicator","Health bar above every nearby player",
                 "healthindicator", ModuleCategory::Visual)
{
    m_settings.defineFloat("range",   "Range (blocks)", 30.f, 5.f, 80.f);
    m_settings.defineFloat("barWidth","Bar Width",       40.f, 20.f,100.f);
    m_settings.defineFloat("barHeight","Bar Height",      5.f,  2.f, 12.f);
    m_settings.defineBool ("showText","Show HP Text",     true);
    m_settings.defineBool ("players", "Players",          true);
    m_settings.defineBool ("mobs",    "Hostile Mobs",     true);
}

void HealthIndicator::onRender(ImDrawList* dl) {
    auto* lp = getLocalPlayer();
    if (!lp) return;

    const float* vp    = ViewProjectionCache::get().get4x4();
    float        range = m_settings.getFloat("range");
    float        bw    = m_settings.getFloat("barWidth");
    float        bh    = m_settings.getFloat("barHeight");
    bool         txt   = m_settings.getBool("showText");

    auto* lvl = getLevel();
    if (!lvl) return;

    for (Actor* e : lvl->getEntityList()) {
        if (!e || !e->isAlive() || e == lp) continue;
        if (e->isPlayer()     && !m_settings.getBool("players")) continue;
        if (e->isHostileMob() && !m_settings.getBool("mobs"))    continue;

        float dist = lp->distanceTo(e);
        if (dist > range) continue;

        // Project a point 2.4 blocks above feet
        Vec3 pos = e->getPosition(); pos.y += 2.4f;
        ImVec2 sp;
        if (!worldToScreen(pos, sp, vp)) continue;

        float ratio = e->getHealthPct();
        ImU32 hcol  = ratio > 0.5f ? IM_COL32(72,199,142,220)
                    : ratio > 0.25f? IM_COL32(255,189,51,220)
                                   : IM_COL32(237,70,70,220);

        float x0 = sp.x - bw * .5f;
        float y0 = sp.y;
        dl->AddRectFilled({x0, y0}, {x0+bw,       y0+bh}, IM_COL32(30,30,30,180), 2.f);
        dl->AddRectFilled({x0, y0}, {x0+bw*ratio, y0+bh}, hcol, 2.f);
        dl->AddRect(      {x0, y0}, {x0+bw,       y0+bh}, IM_COL32(0,0,0,120),    2.f, 0, 1.f);

        if (txt) {
            char buf[12]; snprintf(buf,sizeof(buf),"%.0f",e->getHealth());
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(nullptr,10.f,{sp.x-ts.x*.5f, y0-12.f},IM_COL32(255,255,255,200),buf);
        }
    }
}
