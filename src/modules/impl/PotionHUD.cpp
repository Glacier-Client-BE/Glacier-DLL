#include "PotionHUD.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <array>
#include <cstdio>

struct EffectInfo { int id; const char* icon; const char* name; ImU32 color; };
static constexpr EffectInfo kEffects[] = {
    {  1, ICON_FA_ARROW_UP,    "Speed",        IM_COL32(122, 200,  96, 255) },
    {  2, ICON_FA_ARROW_DOWN,  "Slowness",     IM_COL32(90, 108, 129, 255) },
    {  3, ICON_FA_HAMMER,      "Haste",        IM_COL32(210, 170,  50, 255) },
    {  4, ICON_FA_FIST_RAISED, "Mining Fatigue", IM_COL32(74, 66, 133, 255) },
    {  5, ICON_FA_FIST_RAISED, "Strength",     IM_COL32(147,  36,  35, 255) },
    {  8, ICON_FA_JUMP,        "Jump Boost",   IM_COL32(34, 210,  98, 255) },
    {  9, ICON_FA_DIZZY,       "Nausea",       IM_COL32(85, 115, 72, 255)  },
    { 10, ICON_FA_HEART,       "Regen",        IM_COL32(205,  92, 171, 255) },
    { 11, ICON_FA_SHIELD_ALT,  "Resistance",   IM_COL32(101, 52, 52, 255)  },
    { 12, ICON_FA_FIRE,        "Fire Resist",  IM_COL32(228, 108,  10, 255) },
    { 13, ICON_FA_WATER,       "Water Breath", IM_COL32(46, 82, 153, 255)  },
    { 14, ICON_FA_EYE,         "Invisibility", IM_COL32(127,131,146, 255)  },
    { 15, ICON_FA_EYE_SLASH,   "Blindness",    IM_COL32(31, 31, 35, 255)   },
    { 16, ICON_FA_LIGHTBULB,   "Night Vision", IM_COL32(0, 0, 178, 255)    },
    { 22, ICON_FA_SKULL,       "Poison",       IM_COL32(78, 147, 49, 255)  },
    { 25, ICON_FA_SNOWFLAKE,   "Wither",       IM_COL32(53, 42, 39, 255)   },
    { 28, ICON_FA_BOLT,        "Absorption",   IM_COL32(36, 107, 173, 255) },
};

PotionHUD::PotionHUD()
    : ModuleBase("Potion HUD", "Shows active potion effects with duration",
                 ICON_FA_FLASK, ModuleCategory::HUD, 10.f, 300.f)
{
    m_settings.defineFloat("scale",    "Scale",         1.f, 0.5f, 2.f);
    m_settings.defineBool ("showName", "Show Name",     true);
    m_settings.defineBool ("showTime", "Show Duration", true);
    m_settings.defineBool ("vertical", "Vertical",      true);
    m_settings.defineFloat("bgAlpha",  "BG Alpha",      0.8f, 0.f, 1.f);
}

void PotionHUD::onRenderImGui() {
    auto* lp = getLocalPlayer();
    float sc     = m_settings.getFloat("scale");
    bool  vt     = m_settings.getBool("vertical");
    bool  sname  = m_settings.getBool("showName");
    bool  stime  = m_settings.getBool("showTime");
    float bgAlpha= m_settings.getFloat("bgAlpha");
    float rowH   = 26.f * sc;
    float rowW   = 120.f * sc;

    // Demo: show first 3 effects when not in game
    static const EffectInfo* demo[] = { &kEffects[0], &kEffects[4], &kEffects[9] };
    int numEffects = 3; // would be queried from SDK in real impl

    float totalW = vt ? rowW : rowW * numEffects + 4.f * (numEffects - 1);
    float totalH = vt ? rowH * numEffects + 4.f * (numEffects - 1) : rowH;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ totalW + 8.f, totalH + 8.f });
    ImGui::SetNextWindowBgAlpha(bgAlpha);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg,  ImVec4(0.137f, 0.153f, 0.165f, bgAlpha));
    ImGui::PushStyleColor(ImGuiCol_Border,    ImVec4(0.447f, 0.537f, 0.855f, 0.35f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4.f, 4.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f * sc);

    if (ImGui::Begin("##potionhud", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta;
            m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cur = ImGui::GetWindowPos();
        cur.x += 4.f; cur.y += 4.f;

        for (int i = 0; i < numEffects; i++) {
            const EffectInfo* eff = demo[i];
            ImVec2 rMin = cur;
            ImVec2 rMax = { cur.x + rowW, cur.y + rowH };

            dl->AddRectFilled(rMin, rMax, IM_COL32(44, 47, 51, 200), 5.f * sc);
            dl->AddRect(rMin, rMax, eff->color, 5.f * sc, 0, 1.f);

            // Color dot
            dl->AddCircleFilled({ rMin.x + 8.f * sc, rMin.y + rowH * 0.5f }, 4.f * sc, eff->color);

            float tx = rMin.x + 16.f * sc;
            if (sname) {
                dl->AddText(nullptr, 11.f * sc, { tx, rMin.y + 3.f * sc },
                    IM_COL32(255,255,255,220), eff->name);
            }
            if (stime) {
                char tbuf[16]; snprintf(tbuf, sizeof(tbuf), "0:30");
                dl->AddText(nullptr, 10.f * sc, { tx, rMin.y + rowH - 13.f * sc },
                    IM_COL32(153,170,181,180), tbuf);
            }

            if (vt) cur.y += rowH + 4.f;
            else    cur.x += rowW + 4.f;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
