#include "Hitboxes.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <vector>
#include <cmath>

Hitboxes::Hitboxes()
    : ModuleBase("Hitboxes", "Renders visible hitbox outlines around nearby entities",
                 ICON_FA_VECTOR_SQUARE, ModuleCategory::Visual)
{
    m_settings.defineBool ("players",  "Show Players",    true);
    m_settings.defineBool ("mobs",     "Show Mobs",       true);
    m_settings.defineBool ("passive",  "Show Passive",    false);
    m_settings.defineFloat("range",    "Range",           32.f, 4.f, 64.f);
    m_settings.defineFloat("r",        "Color R",         114.f, 0.f, 255.f);
    m_settings.defineFloat("g",        "Color G",         137.f, 0.f, 255.f);
    m_settings.defineFloat("b",        "Color B",         218.f, 0.f, 255.f);
    m_settings.defineFloat("alpha",    "Alpha",           1.f,   0.f, 1.f);
    m_settings.defineFloat("lineW",    "Line Width",      1.5f,  0.5f, 4.f);
    m_settings.defineBool ("filled",   "Filled Overlay",  false);
    m_settings.defineFloat("fillAlpha","Fill Alpha",      0.1f,  0.f, 0.5f);
}

void Hitboxes::onRender(ImDrawList* dl) {
    auto* lp = getLocalPlayer();
    if (!lp) return;
    auto* lvl = getLevel();
    if (!lvl) return;

    const float* vp  = ViewProjectionCache::get().get4x4();
    if (!vp) return;

    float range   = m_settings.getFloat("range");
    float r       = m_settings.getFloat("r");
    float g       = m_settings.getFloat("g");
    float b       = m_settings.getFloat("b");
    float alpha   = m_settings.getFloat("alpha");
    float lineW   = m_settings.getFloat("lineW");
    bool  filled  = m_settings.getBool("filled");
    float fillAlp = m_settings.getFloat("fillAlpha");
    bool  showPlayers  = m_settings.getBool("players");
    bool  showMobs     = m_settings.getBool("mobs");
    bool  showPassive  = m_settings.getBool("passive");

    ImU32 lineCol = IM_COL32((int)r, (int)g, (int)b, (int)(alpha * 255.f));
    ImU32 fillCol = IM_COL32((int)r, (int)g, (int)b, (int)(fillAlp * 255.f));

    for (Actor* ent : lvl->getEntityList()) {
        if (!ent || ent == lp) continue;
        if (!ent->isAlive()) continue;
        if (lp->distanceTo(ent) > range) continue;

        bool isPlayer  = ent->isPlayer();
        bool isHostile = ent->isHostileMob();
        bool isPassive = ent->isPassiveMob();

        if (isPlayer  && !showPlayers) continue;
        if (isHostile && !showMobs)    continue;
        if (isPassive && !showPassive) continue;

        AABB box = ent->getAABB();
        Vec3 corners[8] = {
            {box.min.x, box.min.y, box.min.z}, {box.max.x, box.min.y, box.min.z},
            {box.max.x, box.min.y, box.max.z}, {box.min.x, box.min.y, box.max.z},
            {box.min.x, box.max.y, box.min.z}, {box.max.x, box.max.y, box.min.z},
            {box.max.x, box.max.y, box.max.z}, {box.min.x, box.max.y, box.max.z},
        };

        ImVec2 sc[8];
        bool allVis = true;
        for (int i = 0; i < 8; i++) {
            if (!worldToScreen(corners[i], sc[i], vp)) { allVis = false; break; }
        }
        if (!allVis) continue;

        static const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };
        for (auto& e : edges)
            dl->AddLine(sc[e[0]], sc[e[1]], lineCol, lineW);

        if (filled) {
            static const int faces[6][4] = {
                {0,1,2,3},{4,5,6,7},{0,1,5,4},
                {2,3,7,6},{1,2,6,5},{0,3,7,4}
            };
            for (auto& f : faces) {
                dl->AddQuadFilled(sc[f[0]], sc[f[1]], sc[f[2]], sc[f[3]], fillCol);
            }
        }
    }
}
