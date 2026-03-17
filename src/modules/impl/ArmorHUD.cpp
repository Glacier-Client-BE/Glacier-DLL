#include "ArmorHUD.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

ArmorHUD::ArmorHUD()
    : ModuleBase("Armor HUD","Equipped armor with durability bars",
                 ICON_FA_SHIELD_ALT, ModuleCategory::HUD, 10.f, 160.f)
{
    m_settings.defineFloat("scale",     "Scale",             1.f, 0.5f, 2.f);
    m_settings.defineBool ("showBars",  "Show Dur Bars",     true);
    m_settings.defineBool ("showNames", "Show Item Names",   false);
    m_settings.defineBool ("showNums",  "Show Dur Numbers",  true);
    m_settings.defineFloat("barWidth",  "Bar Width",        80.f, 30.f, 200.f);
    m_settings.defineFloat("bgAlpha",   "BG Alpha",         0.85f, 0.f, 1.f);
    m_settings.defineBool ("vertical",  "Vertical Layout",   true);
}

ImU32 ArmorHUD::durColor(int d, int m) {
    if (m <= 0) return IM_COL32(153,170,181,255);
    float r = (float)d / m;
    if (r > 0.6f) return IM_COL32(72,199,142,255);
    if (r > 0.3f) return IM_COL32(255,189,51,255);
    return IM_COL32(237,70,70,255);
}

std::array<ArmorSlot,4> ArmorHUD::getSlots() const {
    auto* lp = getLocalPlayer();
    if (!lp) {
        // Fallback preview data when not in-game
        return {{{ICON_FA_HAT_WIZARD " Helmet",   80, 100},
                 {ICON_FA_TSHIRT    " Chestplate",200, 250},
                 {ICON_FA_SOCKS     " Leggings",  140, 225},
                 {ICON_FA_SHOE_PRINTS " Boots",    20,  65}}};
    }
    static const char* slotIcon[4] = {
        ICON_FA_HAT_WIZARD, ICON_FA_TSHIRT, ICON_FA_SOCKS, ICON_FA_SHOE_PRINTS
    };
    static const char* slotName[4] = {"Helmet","Chestplate","Leggings","Boots"};
    std::array<ArmorSlot,4> slots;
    for (int i = 0; i < 4; i++) {
        ItemStack item = lp->getArmorItem(i);
        char buf[32];
        if (item.isValid())
            snprintf(buf, sizeof(buf), "%s %s", slotIcon[i], item.name.empty() ? slotName[i] : item.name.c_str());
        else
            snprintf(buf, sizeof(buf), "%s %s", slotIcon[i], slotName[i]);
        slots[i] = {buf, item.isValid() ? item.getDurability() : 0,
                        item.isValid() ? item.maxDamage       : 0};
    }
    return slots;
}

void ArmorHUD::onRenderImGui() {
    float sc  = m_settings.getFloat("scale");
    float bw  = m_settings.getFloat("barWidth") * sc;
    bool  bars = m_settings.getBool("showBars");
    bool  names= m_settings.getBool("showNames");
    bool  nums = m_settings.getBool("showNums");
    float alpha= m_settings.getFloat("bgAlpha");
    float rh   = 22.f * sc, pad = 6.f * sc, iw = 16.f * sc;
    float totalW = iw + pad + bw + (nums ? 26.f*sc : 0.f) + pad*2;
    float totalH = rh*4 + pad*2;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({totalW, totalH});
    ImGui::SetNextWindowBgAlpha(alpha);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.137f,0.153f,0.165f,alpha));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.447f,0.537f,0.855f,0.4f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  {pad, pad});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  8.f*sc);

    if (ImGui::Begin("##armor", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta; m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cur = ImGui::GetWindowPos(); cur.x += pad; cur.y += pad;

        auto slots = getSlots();
        for (int i = 0; i < 4; i++) {
            auto& s = slots[i];
            bool   hasItem = s.maxDur > 0;
            ImU32  dc      = durColor(s.dur, s.maxDur);
            float  ratio   = hasItem ? (float)s.dur / s.maxDur : 0.f;

            // Icon badge
            ImVec2 iTL = cur, iBR = {cur.x+iw, cur.y+rh-pad*.5f};
            dl->AddRectFilled(iTL, iBR, IM_COL32(44,47,51,220), 4);
            dl->AddRect(iTL, iBR, hasItem ? dc : IM_COL32(60,64,70,200), 4);

            // Slot letter / icon
            const char* icn = s.name.c_str(); // first char is icon
            ImVec2 ts = ImGui::CalcTextSize(icn);
            dl->AddText({iTL.x+(iw-ts.x)*.5f, iTL.y+(iBR.y-iTL.y-ts.y)*.5f},
                        hasItem ? IM_COL32(255,255,255,255) : IM_COL32(80,85,95,200), icn);

            float tx = cur.x + iw + pad, ty = cur.y;
            if (names) {
                dl->AddText(nullptr, 11*sc, {tx,ty}, IM_COL32(153,170,181,200), s.name.c_str());
                ty += 13.f*sc;
            }
            if (bars && hasItem) {
                ImVec2 tl{tx, ty+(rh-pad*.5f-6*sc)*.5f}, br{tx+bw, tl.y+6*sc};
                dl->AddRectFilled(tl, br, IM_COL32(35,39,42,255), 3);
                dl->AddRectFilled(tl, {tl.x+bw*ratio, br.y}, dc, 3);
                if (nums) {
                    char nb[12];
                    snprintf(nb, sizeof(nb), "%d", s.dur);
                    dl->AddText(nullptr, 10*sc, {br.x+3, tl.y}, IM_COL32(153,170,181,200), nb);
                }
            } else if (!hasItem) {
                dl->AddText(nullptr, 10*sc, {tx, ty+(rh-pad*.5f-10*sc)*.5f},
                            IM_COL32(60,65,70,180), "Empty");
            }
            cur.y += rh;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
