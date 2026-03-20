#include "PotionHUD.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>

struct EffDef { const char* name; ImU32 col; };
static constexpr EffDef kEffects[] = {
    {"Speed",         IM_COL32(122,200, 96,255)},
    {"Slowness",      IM_COL32( 90,108,129,255)},
    {"Haste",         IM_COL32(210,170, 50,255)},
    {"Mining Fatigue",IM_COL32( 74, 66,133,255)},
    {"Strength",      IM_COL32(147, 36, 35,255)},
    {"Jump Boost",    IM_COL32( 34,210, 98,255)},
    {"Nausea",        IM_COL32( 85,115, 72,255)},
    {"Regen",         IM_COL32(205, 92,171,255)},
    {"Resistance",    IM_COL32(101, 52, 52,255)},
    {"Fire Resist",   IM_COL32(228,108, 10,255)},
    {"Water Breath",  IM_COL32( 46, 82,153,255)},
    {"Invisibility",  IM_COL32(127,131,146,255)},
    {"Night Vision",  IM_COL32(  0,  0,178,255)},
    {"Poison",        IM_COL32( 78,147, 49,255)},
    {"Wither",        IM_COL32( 53, 42, 39,255)},
    {"Absorption",    IM_COL32( 36,107,173,255)},
};

PotionHUD::PotionHUD()
    : ModuleBase("Potion HUD","Active effects with duration bars",
                 "potionhud", ModuleCategory::HUD, 10.f, 300.f)
{
    m_settings.defineBool ("showDur",  "Duration Bar",    true);
    m_settings.defineBool ("showName", "Effect Name",     true);
    m_settings.defineBool ("vertical", "Vertical",        true);
    m_settings.defineFloat("scale",    "Scale",           1.f, 0.5f, 2.f);
}
void PotionHUD::onRenderImGui() {
    // Demo effects — real build queries lp->getActiveEffects()
    struct AE { int idx; int ticks; int amp; };
    static const AE demo[] = { {0,400,1}, {4,600,0}, {7,200,0} };
    int num=3; // show demo when not in-game

    auto* lp=getLocalPlayer();
    if (lp) num=0; // hide demo when in-game; real impl queries SDK
    if (!num) return;

    float sc=m_settings.getFloat("scale");
    bool  vt=m_settings.getBool("vertical");
    bool  sN=m_settings.getBool("showName");
    bool  sD=m_settings.getBool("showDur");
    float rowH=(14.f+(sD?8.f:0.f)+6.f)*sc;
    float rowW=sN?130.f*sc:32.f*sc;
    float totW=vt?rowW+HUDStyle::PAD_X*2:(rowW+4.f)*num+HUDStyle::PAD_X;
    float totH=vt?(rowH+4.f)*num+HUDStyle::PAD_Y*2:rowH+HUDStyle::PAD_Y*2;

    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({totW,totH});
    HUDStyle::push();
    if (ImGui::Begin("##pot",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 base=ImGui::GetWindowPos();
        float cx=base.x+HUDStyle::PAD_X, cy=base.y+HUDStyle::PAD_Y;
        for (int i=0;i<num;i++) {
            const AE& ae=demo[i];
            const EffDef& ef=kEffects[ae.idx];
            ImVec2 rMin={cx,cy}, rMax={cx+rowW,cy+rowH};
            dl->AddRectFilled(rMin,rMax,IM_COL32(20,20,20,200),6.f*sc);
            dl->AddRect(rMin,rMax,ef.col,6.f*sc,0,1.2f);
            // Color dot
            float dotR=5.f*sc;
            dl->AddCircleFilled({cx+8.f*sc,cy+rowH*0.5f},dotR,ef.col);
            if (sN) {
                char nb[32];
                if (ae.amp>0) snprintf(nb,sizeof(nb),"%s %d",ef.name,ae.amp+1);
                else snprintf(nb,sizeof(nb),"%s",ef.name);
                HUDStyle::text(dl,12.f*sc,{cx+18.f*sc,cy+3.f*sc},HUDStyle::WHITE,nb,false);
                if (sD){
                    int secs=ae.ticks/20;
                    char tb[12]; snprintf(tb,sizeof(tb),"%d:%02d",secs/60,secs%60);
                    HUDStyle::text(dl,HUDStyle::FONT_SMALL*sc,{cx+18.f*sc,cy+3.f*sc+13.f*sc},HUDStyle::GREY,tb,false);
                    float ratio=std::min((float)ae.ticks/1200.f,1.f);
                    float bx=cx+18.f*sc, by=cy+3.f*sc+13.f*sc+12.f*sc;
                    HUDStyle::bar(dl,bx,by,rowW-18.f*sc-6.f*sc,4.f*sc,ratio,ef.col,2.f);
                }
            }
            if (vt) cy+=rowH+4.f;
            else    cx+=rowW+4.f;
        }
    }
    ImGui::End(); HUDStyle::pop();
}
