#include "InventoryViewer.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>

InventoryViewer::InventoryViewer()
    : ModuleBase("Inventory","Floating inventory with durability bars",
                 "inventoryviewer", ModuleCategory::HUD, 200.f, 10.f)
{
    m_settings.defineFloat("scale",    "Scale",          1.f,  0.5f, 2.f);
    m_settings.defineBool ("showHeld", "Highlight Held",  true);
    m_settings.defineBool ("showDur",  "Durability Bars",  true);
    m_settings.defineBool ("showCount","Item Count",        true);
    m_settings.defineBool ("showArmor","Armor Row",         true);
    m_settings.defineBool ("compact",  "Hotbar Only",       false);
}
void InventoryViewer::onRenderImGui() {
    float sc=m_settings.getFloat("scale");
    bool  hl=m_settings.getBool("showHeld"),sd=m_settings.getBool("showDur"),
          sc2=m_settings.getBool("showCount"),sa=m_settings.getBool("showArmor"),
          cmp=m_settings.getBool("compact");
    const int cols=9; int rows=cmp?1:4; int armR=sa&&!cmp?1:0;
    float cell=26.f*sc, gap=2.f*sc, pad=8.f*sc;
    float panW=cols*(cell+gap)-gap+pad*2;
    float panH=(rows+armR)*(cell+gap)-gap+pad*2;
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW,panH});
    HUDStyle::push();
    if (ImGui::Begin("##inv",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 base=ImGui::GetWindowPos(); base.x+=pad; base.y+=pad;
        auto* lp=getLocalPlayer();
        int held=lp?lp->getSelectedSlot():-1;
        auto drawSlot=[&](int slot,float cx,float cy,bool hotbar,int hcol){
            ItemStack it=lp?lp->getInventoryItem(slot):ItemStack{};
            ImVec2 tl{cx,cy},br{cx+cell,cy+cell};
            bool isHeld=hl&&hotbar&&hcol==held;
            ImU32 bg=isHeld?IM_COL32(114,137,218,55):IM_COL32(30,30,30,210);
            ImU32 bd=isHeld?HUDStyle::ACCENT:IM_COL32(55,60,68,160);
            dl->AddRectFilled(tl,br,bg,4.f);
            dl->AddRect(tl,br,bd,4.f,0,1.f);
            if (it.isValid()) {
                if (sc2&&it.count>1){ char c[8]; snprintf(c,sizeof(c),"%d",it.count);
                    ImVec2 ts=ImGui::CalcTextSize(c); float fss=9.f*sc;
                    dl->AddText(nullptr,fss,{br.x-ts.x*fss/ImGui::GetFontSize()-1.f,br.y-fss-1.f},HUDStyle::WHITE,c); }
                if (sd&&it.hasDurability()){
                    float r=it.getDurabilityPct();
                    ImU32 dc=r>0.6f?HUDStyle::GREEN:r>0.3f?HUDStyle::YELLOW:HUDStyle::RED;
                    HUDStyle::bar(dl,tl.x,br.y-3.f*sc,cell,2.5f*sc,r,dc,1.f);
                }
            }
        };
        for (int c=0;c<cols;c++) drawSlot(c,base.x+c*(cell+gap),base.y,true,c);
        if (!cmp) {
            for (int r=1;r<rows;r++) for (int c=0;c<cols;c++)
                drawSlot(9+(r-1)*9+c,base.x+c*(cell+gap),base.y+r*(cell+gap),false,-1);
            if (armR) for (int i=0;i<4;i++){
                float cx=base.x+i*(cell+gap), cy=base.y+rows*(cell+gap);
                ItemStack arm=lp?lp->getArmorItem(i):ItemStack{};
                ImVec2 tl{cx,cy},br{cx+cell,cy+cell};
                ImU32 bd=arm.isValid()?HUDStyle::ACCENT:IM_COL32(55,60,68,140);
                dl->AddRectFilled(tl,br,IM_COL32(30,30,30,210),4.f);
                dl->AddRect(tl,br,bd,4.f,0,1.f);
                if (sd&&arm.isValid()&&arm.hasDurability())
                    HUDStyle::bar(dl,tl.x,br.y-3.f*sc,cell,2.5f*sc,arm.getDurabilityPct(),HUDStyle::ACCENT,1.f);
            }
        }
    }
    ImGui::End(); HUDStyle::pop();
}
