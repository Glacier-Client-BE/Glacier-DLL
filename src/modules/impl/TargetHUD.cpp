#include "TargetHUD.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

TargetHUD::TargetHUD()
    : ModuleBase("Target HUD", "Detailed target info panel with animated health bar",
                 "targethud", ModuleCategory::Combat, 300.f, 200.f)
{
    m_settings.defineBool ("showHealth",  "Health Bar",         true);
    m_settings.defineBool ("showDist",    "Distance",           true);
    m_settings.defineBool ("showArmor",   "Armor",              true);
    m_settings.defineBool ("showType",    "Entity Type",        true);
    m_settings.defineBool ("showName",    "Name",               true);
    m_settings.defineBool ("animate",     "Animate Health",     true);
    m_settings.defineBool ("fadeNoTgt",   "Hide When No Target",true);
    m_settings.defineFloat("scale",       "Scale",              1.f,  0.5f, 2.f);
}

void TargetHUD::onTick() { TargetTracker::get().update(getLocalPlayer()); }

void TargetHUD::onRenderImGui() {
    auto&  tt  = TargetTracker::get();
    Actor* tgt = tt.target;
    if (!tgt && m_settings.getBool("fadeNoTgt")) return;

    bool  hasTgt = tgt != nullptr;
    float hp = 20.f, maxHp = 20.f, dist = 0.f; int armor = 0;
    std::string name = "No Target";
    if (hasTgt) { name=tgt->getName(); hp=tgt->getHealth(); maxHp=tgt->getMaxHealth();
                  dist=tt.distance; armor=tgt->getArmorValue(); }

    if (name != m_lastName) { m_lastName=name; m_fadeIn=0.f; }
    m_fadeIn = std::min(m_fadeIn + ImGui::GetIO().DeltaTime*5.f, 1.f);

    float tgt_ = maxHp>0.f ? hp/maxHp : 0.f;
    if (m_settings.getBool("animate"))
        m_dispHP += (tgt_ - m_dispHP) * 0.12f;
    else m_dispHP = tgt_;

    float sc = m_settings.getFloat("scale");
    ImU32 accent = HUDStyle::ACCENT;
    const char* typeStr = "Entity";
    if (hasTgt) {
        if (tgt->isPlayer())    { accent=HUDStyle::GREEN;  typeStr="Player"; }
        else if (tgt->isHostileMob()) { accent=HUDStyle::RED;    typeStr="Hostile"; }
        else if (tgt->isPassiveMob()) { accent=HUDStyle::YELLOW; typeStr="Passive"; }
    }

    float panW = 200.f * sc;
    float panH = ( 6.f
        + (m_settings.getBool("showName")  ? 20.f : 0.f)
        + (m_settings.getBool("showType")  ? 14.f : 0.f)
        + (m_settings.getBool("showHealth")? 26.f : 0.f)
        + (m_settings.getBool("showDist")  ? 14.f : 0.f)
        + (m_settings.getBool("showArmor") ? 14.f : 0.f)
        ) * sc + HUDStyle::PAD_Y;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ panW, panH });
    float a = m_fadeIn * (m_settings.getBool("fadeNoTgt") ? 1.f : 1.f);
    HUDStyle::push(HUDStyle::BG_ALPHA * a);
    if (ImGui::Begin("##tgt", nullptr, HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp = ImGui::GetWindowPos();
        float bx = bp.x + 8.f*sc, oy = 6.f*sc;

        // Accent side bar
        dl->AddRectFilled({ bp.x, bp.y+8.f*sc },{ bp.x+3.f*sc, bp.y+panH-8.f*sc }, accent, 2.f);

        if (m_settings.getBool("showName")) {
            HUDStyle::text(dl, HUDStyle::FONT_BIG*sc, {bx,bp.y+oy},
                hasTgt ? HUDStyle::WHITE : HUDStyle::GREY, name.c_str());
            oy += 20.f*sc;
        }
        if (m_settings.getBool("showType") && hasTgt) {
            HUDStyle::text(dl,HUDStyle::FONT_SMALL*sc,{bx,bp.y+oy},accent,typeStr,false);
            oy += 14.f*sc;
        }
        if (m_settings.getBool("showHealth")) {
            float bw = panW - bx - HUDStyle::PAD_X;
            ImU32 hcol = m_dispHP>0.5f?HUDStyle::GREEN:m_dispHP>0.25f?HUDStyle::YELLOW:HUDStyle::RED;
            HUDStyle::bar(dl, bx, bp.y+oy, bw, 8.f*sc, m_dispHP, hcol, 4.f);
            if (hasTgt) {
                char hb[32]; snprintf(hb,sizeof(hb),"%.0f / %.0f HP",hp,maxHp);
                HUDStyle::text(dl,HUDStyle::FONT_SMALL*sc,{bx,bp.y+oy+10.f*sc},HUDStyle::GREY,hb,false);
            }
            oy += 26.f*sc;
        }
        if (m_settings.getBool("showDist") && hasTgt) {
            char db[32]; snprintf(db,sizeof(db),"%.1f blocks",dist);
            HUDStyle::text(dl,HUDStyle::FONT_SMALL*sc,{bx,bp.y+oy},HUDStyle::GREY,db,false);
            oy += 14.f*sc;
        }
        if (m_settings.getBool("showArmor") && hasTgt) {
            char ab[24]; snprintf(ab,sizeof(ab),"%d armor pts",armor);
            HUDStyle::text(dl,HUDStyle::FONT_SMALL*sc,{bx,bp.y+oy},accent,ab,false);
        }
    }
    ImGui::End();
    HUDStyle::pop();
}
