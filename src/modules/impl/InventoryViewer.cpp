#include "InventoryViewer.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>

InventoryViewer::InventoryViewer()
    : ModuleBase("Inventory Viewer","Shows your inventory in a floating panel",
                 ICON_FA_ARCHIVE, ModuleCategory::HUD, 200.f, 10.f)
{
    m_settings.defineFloat("scale",    "Scale",          1.f,  0.5f, 2.f);
    m_settings.defineFloat("bgAlpha",  "BG Alpha",       0.88f, 0.f, 1.f);
    m_settings.defineBool ("showHeld", "Highlight Held", true);
    m_settings.defineBool ("showDur",  "Show Durability",true);
    m_settings.defineBool ("showNames","Show Item Names", false);
}

void InventoryViewer::onRenderImGui() {
    float sc    = m_settings.getFloat("scale");
    float alpha = m_settings.getFloat("bgAlpha");
    bool  showDur  = m_settings.getBool("showDur");
    bool  showNames= m_settings.getBool("showNames");
    bool  hilite   = m_settings.getBool("showHeld");

    // 9 hotbar + 27 inventory = 36 slots, displayed as 4 rows of 9
    const int cols = 9, rows = 4;
    float cell = 28.f * sc;
    float pad  = 6.f  * sc;
    float panW = cols * cell + pad * 2.f;
    float panH = rows * cell + pad * 2.f + (showNames ? 14.f*sc : 0.f);

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW, panH});
    ImGui::SetNextWindowBgAlpha(alpha);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.137f,0.153f,0.165f,alpha));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.447f,0.537f,0.855f,0.4f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {pad, pad});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f*sc);

    if (ImGui::Begin("##inv", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta; m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      base= ImGui::GetWindowPos();
        base.x += pad; base.y += pad;

        auto* lp       = getLocalPlayer();
        int   heldSlot = lp ? lp->getSelectedSlot() : -1;

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                // Row 0 = hotbar (slots 0-8), rows 1-3 = main inventory (9-35)
                int slot = (row == 0) ? col : (row * 9 + col);

                ItemStack item;
                if (lp) {
                    if (slot < 9)       item = lp->getInventoryItem(slot);
                    else {
                        // Armor in first row override: use last 4 slots of row0 as armor preview
                        item = lp->getInventoryItem(slot);
                    }
                }

                float cx = base.x + col * cell;
                float cy = base.y + row * cell;
                ImVec2 tl{cx, cy}, br{cx+cell-2, cy+cell-2};

                // Cell background
                bool isHeld = (hilite && row == 0 && col == heldSlot);
                ImU32 cellBg = isHeld
                    ? IM_COL32(114,137,218,60)
                    : IM_COL32(44,47,51,200);
                ImU32 cellBorder = isHeld
                    ? IM_COL32(114,137,218,180)
                    : IM_COL32(60,65,72,180);

                dl->AddRectFilled(tl, br, cellBg, 3.f);
                dl->AddRect(tl, br, cellBorder, 3.f, 0, 1.f);

                if (item.isValid()) {
                    // Item count badge
                    if (item.count > 1) {
                        char cnt[8]; snprintf(cnt, sizeof(cnt), "%d", item.count);
                        ImVec2 ts = ImGui::CalcTextSize(cnt);
                        dl->AddText(nullptr, 9.f*sc,
                                    {br.x - ts.x - 1.f, br.y - 10.f*sc},
                                    IM_COL32(255,255,255,255), cnt);
                    }

                    // Durability bar
                    if (showDur && item.hasDurability()) {
                        float ratio = item.getDurabilityPct();
                        ImU32 dc = ratio > 0.6f ? IM_COL32(72,199,142,220)
                                 : ratio > 0.3f ? IM_COL32(255,189,51,220)
                                                : IM_COL32(237,70,70,220);
                        float bw = cell - 4.f;
                        dl->AddRectFilled({tl.x+1, br.y-4.f}, {tl.x+1+bw,     br.y-1.f}, IM_COL32(30,30,30,200), 1.f);
                        dl->AddRectFilled({tl.x+1, br.y-4.f}, {tl.x+1+bw*ratio,br.y-1.f}, dc, 1.f);
                    }

                    // Item name label (abbreviated to fit cell)
                    if (showNames && !item.name.empty()) {
                        std::string abbr = item.name.substr(0, 3);
                        dl->AddText(nullptr, 8.f*sc, {tl.x+2, tl.y+2},
                                    IM_COL32(200,200,200,180), abbr.c_str());
                    }
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
