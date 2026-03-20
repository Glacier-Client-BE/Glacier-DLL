#include "ArmorHUD.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>

static const char* kArmIcons[4]  = { "\xef\x80\x83", "\xef\x84\xa1", "\xef\x85\xa7", "\xef\x95\xb4" }; // FA glyphs via raw UTF8 fallback
static const char* kArmLabels[4] = { "Helmet", "Chestplate", "Leggings", "Boots" };

static ImU32 durColor(int d, int m) {
    if (m <= 0) return HUDStyle::GREY;
    float r = (float)d / m;
    return r > 0.6f ? HUDStyle::GREEN : r > 0.3f ? HUDStyle::YELLOW : HUDStyle::RED;
}
static float durRatio(int d, int m) {
    if (m <= 0) return 0.f;
    float r = (float)d / m;
    return r < 0.f ? 0.f : r > 1.f ? 1.f : r;
}

std::array<ArmorSlot, 5> ArmorHUD::getSlots() const {
    auto* lp = getLocalPlayer();
    std::array<ArmorSlot, 5> slots;
    for (int i = 0; i < 4; i++) {
        ItemStack item = lp ? lp->getArmorItem(i) : ItemStack{};
        slots[i] = { kArmIcons[i], kArmLabels[i],
                     item.isValid() ? item.getDurability() : 0,
                     item.isValid() ? item.maxDamage       : 0,
                     item.isValid(), item.name };
    }
    ItemStack off = lp ? lp->getInventoryItem(40) : ItemStack{};
    slots[4] = { "\xef\x82\xa5", "Offhand",
                 off.isValid() ? off.getDurability() : 0,
                 off.isValid() ? off.maxDamage       : 0,
                 off.isValid(), off.name };
    return slots;
}

ArmorHUD::ArmorHUD()
    : ModuleBase("Armor HUD", "Armor durability with big icons and low-dur pulse warning",
                 "armorhud", ModuleCategory::HUD, 10.f, 160.f)
{
    m_settings.defineFloat("scale",       "Scale",            1.f,   0.5f, 2.f);
    m_settings.defineBool ("showBars",    "Dur Bars",         true);
    m_settings.defineBool ("showNums",    "Dur Numbers",      true);
    m_settings.defineBool ("showEmpty",   "Show Empty Slots",  true);
    m_settings.defineBool ("showOffhand", "Show Offhand",      true);
    m_settings.defineBool ("lowWarn",     "Low-Dur Warning",   true);
    m_settings.defineFloat("lowPct",      "Warn Below %",     25.f, 1.f, 60.f);
    m_settings.defineFloat("barLen",      "Bar Length",       72.f, 24.f,180.f);
}

void ArmorHUD::onRenderImGui() {
    float sc     = m_settings.getFloat("scale");
    float barLen = m_settings.getFloat("barLen") * sc;
    bool  bars   = m_settings.getBool("showBars");
    bool  nums   = m_settings.getBool("showNums");
    bool  showEmp= m_settings.getBool("showEmpty");
    bool  showOff= m_settings.getBool("showOffhand");
    bool  lw     = m_settings.getBool("lowWarn");
    float lwPct  = m_settings.getFloat("lowPct") / 100.f;

    float iconW = 28.f * sc;
    float rowH  = iconW + 4.f * sc;
    float numW  = nums ? 34.f * sc : 0.f;
    float barH  = 7.f * sc;

    auto slots   = getSlots();
    int  count   = showOff ? 5 : 4;
    int  vis     = 0;
    for (int i = 0; i < count; i++) if (showEmp || slots[i].hasItem) vis++;
    if (vis == 0) return;

    float panW = HUDStyle::PAD_X + iconW + 8.f * sc + barLen + numW + HUDStyle::PAD_X;
    float panH = rowH * vis + HUDStyle::PAD_Y * 2 + (vis - 1) * 4.f * sc;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ panW, panH });
    HUDStyle::push();

    if (ImGui::Begin("##armor", nullptr, HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 base = ImGui::GetWindowPos();
        base.x += HUDStyle::PAD_X; base.y += HUDStyle::PAD_Y;

        float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 6.f);
        int drawn = 0;

        for (int i = 0; i < count; i++) {
            auto& s = slots[i];
            if (!showEmp && !s.hasItem) continue;

            float oy = drawn * (rowH + 4.f * sc);
            ImU32 dc = durColor(s.dur, s.maxDur);
            float ratio = durRatio(s.dur, s.maxDur);
            bool  isLow = lw && s.hasItem && ratio > 0.f && ratio < lwPct;

            // Icon tile
            ImVec2 iTL = { base.x, base.y + oy };
            ImVec2 iBR = { iTL.x + iconW, iTL.y + iconW };
            ImU32  bg2  = isLow
                ? IM_COL32(60, 10, 10, (int)(180 + 60 * pulse))
                : IM_COL32(30, 30, 30, 220);
            ImU32  bdr  = isLow
                ? IM_COL32(237, 70, 70, (int)(100 + 155 * pulse))
                : (s.hasItem ? dc : IM_COL32(55, 60, 68, 180));
            dl->AddRectFilled(iTL, iBR, bg2, 6.f * sc);
            dl->AddRect(iTL, iBR, bdr, 6.f * sc, 0, isLow ? 2.f : 1.f);

            // Centered icon letter (using slot label first char as fallback)
            char fallback[2] = { s.label[0], 0 };
            ImVec2 iSz = ImGui::CalcTextSize(fallback);
            float  fs2  = iconW * 0.5f;
            dl->AddText(ImGui::GetFont(), fs2,
                { iTL.x + (iconW - iSz.x * fs2 / ImGui::GetFontSize()) * 0.5f,
                  iTL.y + (iconW - iSz.y * fs2 / ImGui::GetFontSize()) * 0.5f },
                s.hasItem ? dc : IM_COL32(70, 75, 85, 160), fallback);

            float tx = base.x + iconW + 8.f * sc;

            if (s.hasItem) {
                // Name
                if (!s.itemName.empty()) {
                    HUDStyle::text(dl, HUDStyle::FONT_SMALL * sc,
                        { tx, base.y + oy + 1.f }, HUDStyle::GREY, s.itemName.c_str(), false);
                }

                if (bars) {
                    float by = base.y + oy + iconW * 0.5f - barH * 0.5f;
                    if (!s.itemName.empty()) by = base.y + oy + HUDStyle::FONT_SMALL * sc + 4.f;
                    HUDStyle::bar(dl, tx, by, barLen, barH, ratio, dc, 4.f);
                    if (nums) {
                        char nb[12]; snprintf(nb, sizeof(nb), "%d", s.dur);
                        HUDStyle::text(dl, HUDStyle::FONT_SMALL * sc,
                            { tx + barLen + 4.f, by }, HUDStyle::GREY, nb, false);
                    }
                }
            } else {
                HUDStyle::text(dl, HUDStyle::FONT_SMALL * sc,
                    { tx, base.y + oy + iconW * 0.5f - HUDStyle::FONT_SMALL * sc * 0.5f },
                    IM_COL32(60,65,72,180), "Empty", false);
            }

            drawn++;
        }
    }
    ImGui::End();
    HUDStyle::pop();
}
