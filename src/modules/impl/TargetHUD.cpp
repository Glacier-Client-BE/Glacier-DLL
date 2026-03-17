#include "TargetHUD.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>
#include <cmath>

TargetHUD::TargetHUD()
    : ModuleBase("Target HUD","Shows info about the entity you're targeting",
                 ICON_FA_CROSSHAIRS, ModuleCategory::Combat, 300.f, 200.f)
{
    m_settings.defineBool ("showHealth",     "Show Health Bar",    true);
    m_settings.defineBool ("showDistance",   "Show Distance",      true);
    m_settings.defineBool ("showArmor",      "Show Armor Points",  true);
    m_settings.defineBool ("showName",       "Show Name",          true);
    m_settings.defineFloat("scale",          "Scale",             1.f, 0.5f, 2.f);
    m_settings.defineFloat("bgAlpha",        "BG Alpha",          0.85f, 0.f, 1.f);
    m_settings.defineBool ("fadeOnNoTarget", "Hide When No Target",true);
    m_settings.defineBool ("animateHealth",  "Animate Health Bar", true);
}

void TargetHUD::onTick() {
    // Update target tracker every tick
    auto* lp = getLocalPlayer();
    TargetTracker::get().update(lp);
}

void TargetHUD::onRenderImGui() {
    auto& tt = TargetTracker::get();
    Actor* target = tt.target;

    if (!target && m_settings.getBool("fadeOnNoTarget")) return;

    // Populate data: from SDK or fallback preview
    std::string targetName = "No Target";
    float health = 20.f, maxHealth = 20.f, distance = 0.f;
    int   armor  = 0;
    bool  hasTarget = (target != nullptr);

    if (hasTarget) {
        targetName = target->getName();
        health     = target->getHealth();
        maxHealth  = target->getMaxHealth();
        distance   = tt.distance;
        armor      = target->getArmorValue();
    }

    // Smooth health bar animation
    if (m_settings.getBool("animateHealth")) {
        float targetRatio = maxHealth > 0.f ? health / maxHealth : 0.f;
        m_displayedHealthRatio += (targetRatio - m_displayedHealthRatio) * 0.15f;
    } else {
        m_displayedHealthRatio = maxHealth > 0.f ? health / maxHealth : 0.f;
    }

    float sc    = m_settings.getFloat("scale");
    float alpha = m_settings.getFloat("bgAlpha");
    float panW  = 175.f * sc;
    float panH  = (6.f + (m_settings.getBool("showName") ? 16.f : 0.f)
                       + (m_settings.getBool("showHealth") ? 22.f : 0.f)
                       + (m_settings.getBool("showDistance") ? 14.f : 0.f)
                       + (m_settings.getBool("showArmor") ? 14.f : 0.f)) * sc;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW, panH});
    ImGui::SetNextWindowBgAlpha(alpha);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.137f,0.153f,0.165f,alpha));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.447f,0.537f,0.855f,0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  {8*sc, 6*sc});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  8*sc);

    if (ImGui::Begin("##targethud", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta; m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      bp  = ImGui::GetWindowPos();
        float pad = 8.f*sc, oy = 6.f*sc;

        if (m_settings.getBool("showName")) {
            dl->AddText(nullptr, 13*sc, {bp.x+pad, bp.y+oy},
                        hasTarget ? IM_COL32(255,255,255,255) : IM_COL32(100,100,100,180),
                        targetName.c_str());
            oy += 16.f*sc;
        }

        if (m_settings.getBool("showHealth")) {
            float barW = panW - pad*2;
            float ratio = m_displayedHealthRatio;
            ImU32 hcol = ratio > 0.5f ? IM_COL32(72,199,142,255)
                       : ratio > 0.25f? IM_COL32(255,189,51,255)
                       :                IM_COL32(237,70,70,255);
            dl->AddRectFilled({bp.x+pad,  bp.y+oy}, {bp.x+pad+barW,   bp.y+oy+8*sc}, IM_COL32(35,39,42,255), 3*sc);
            dl->AddRectFilled({bp.x+pad,  bp.y+oy}, {bp.x+pad+barW*ratio, bp.y+oy+8*sc}, hcol, 3*sc);
            if (hasTarget) {
                char hbuf[28]; snprintf(hbuf, sizeof(hbuf), "%.0f / %.0f HP", (double)health, (double)maxHealth);
                dl->AddText(nullptr, 10*sc, {bp.x+pad+2, bp.y+oy+9*sc}, IM_COL32(153,170,181,200), hbuf);
            }
            oy += 22.f*sc;
        }

        if (m_settings.getBool("showDistance") && hasTarget) {
            char dbuf[28]; snprintf(dbuf, sizeof(dbuf), "%.1f blocks away", (double)distance);
            dl->AddText(nullptr, 10*sc, {bp.x+pad, bp.y+oy}, IM_COL32(153,170,181,180), dbuf);
            oy += 13.f*sc;
        }

        if (m_settings.getBool("showArmor") && hasTarget) {
            char abuf[28]; snprintf(abuf, sizeof(abuf), ICON_FA_SHIELD_ALT "  %d / 20", armor);
            dl->AddText(nullptr, 10*sc, {bp.x+pad, bp.y+oy}, IM_COL32(114,137,218,200), abuf);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
