#include "SpeedHUD.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

SpeedHUD::SpeedHUD()
    : ModuleBase("Speed HUD", "Movement speed in BPS and KPH with sprint-color and graph",
                 "speedhud", ModuleCategory::HUD, 10.f, 290.f)
{
    m_settings.defineBool ("showKPH",    "Show KPH",        true);
    m_settings.defineBool ("showMax",    "Show Max",        true);
    m_settings.defineBool ("showGraph",  "Show Graph",      true);
    m_settings.defineBool ("sprintCol",  "Sprint Color",    true);
    m_settings.defineFloat("graphW",     "Graph Width",     90.f, 40.f, 200.f);
    m_settings.defineFloat("graphH",     "Graph Height",    24.f, 12.f,  60.f);
    m_settings.defineInt  ("samples",    "Graph Samples",   50,   10,   120);
    m_settings.defineFloat("scale",      "Scale",           1.f,  0.5f,  2.f);
    m_settings.defineBool ("resetMax",   "Reset Max",       true);
    m_lastTime = std::chrono::high_resolution_clock::now();
}

void SpeedHUD::onEnable() {
    if (m_settings.getBool("resetMax")) m_maxBps = 0.f;
    m_history.clear();
    m_lastTime = std::chrono::high_resolution_clock::now();
    auto* lp = getLocalPlayer();
    if (lp) { Vec3 p=lp->getPosition(); m_lastX=p.x; m_lastZ=p.z; }
}

void SpeedHUD::onRenderImGui() {
    auto  now = std::chrono::high_resolution_clock::now();
    float dt  = std::chrono::duration<float>(now - m_lastTime).count();
    m_lastTime = now;
    auto* lp   = getLocalPlayer();
    float cx = m_lastX, cz = m_lastZ;
    bool  spr = false;
    if (lp) { Vec3 p=lp->getPosition(); cx=p.x; cz=p.z; spr=lp->isSprinting(); }
    if (dt>0.f&&dt<1.f) {
        float dx=cx-m_lastX,dz=cz-m_lastZ;
        m_bps = sqrtf(dx*dx+dz*dz)/dt;
        if (m_bps>m_maxBps) m_maxBps=m_bps;
        int ms=m_settings.getInt("samples");
        m_history.push_back(m_bps);
        while ((int)m_history.size()>ms) m_history.pop_front();
    }
    m_lastX=cx; m_lastZ=cz;

    float sc  = m_settings.getFloat("scale");
    float gw  = m_settings.getFloat("graphW") * sc;
    float gh  = m_settings.getFloat("graphH") * sc;
    bool  sK  = m_settings.getBool("showKPH");
    bool  sM  = m_settings.getBool("showMax");
    bool  sG  = m_settings.getBool("showGraph");

    ImU32 col = (m_settings.getBool("sprintCol") && spr) ? HUDStyle::GREEN : HUDStyle::ACCENT;
    if (spr && m_maxBps>0.f && m_bps/m_maxBps>0.9f) col = HUDStyle::YELLOW;

    float panW = std::max(gw + HUDStyle::PAD_X*2, 110.f*sc);
    float panH = HUDStyle::FONT_BIG*sc
        + (sK ? HUDStyle::FONT_MID*sc+3.f : 0.f)
        + (sM ? HUDStyle::FONT_SMALL*sc+3.f : 0.f)
        + (sG ? gh+6.f : 0.f)
        + HUDStyle::PAD_Y*2;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ panW, panH });
    HUDStyle::push();
    if (ImGui::Begin("##spd", nullptr, HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp = ImGui::GetWindowPos();
        float  oy = HUDStyle::PAD_Y;
        float  bx = bp.x + HUDStyle::PAD_X;
        char   buf[48];
        snprintf(buf,sizeof(buf),"%.2f BPS",m_bps);
        HUDStyle::text(dl, HUDStyle::FONT_BIG*sc, {bx,bp.y+oy}, col, buf);
        oy += HUDStyle::FONT_BIG*sc + 3.f;
        if (sK) { snprintf(buf,sizeof(buf),"%.1f KPH",m_bps*3.6f);
            HUDStyle::text(dl,HUDStyle::FONT_MID*sc,{bx,bp.y+oy},HUDStyle::GREY,buf,false); oy+=HUDStyle::FONT_MID*sc+3.f; }
        if (sM) { snprintf(buf,sizeof(buf),"Max %.2f",m_maxBps);
            HUDStyle::text(dl,HUDStyle::FONT_SMALL*sc,{bx,bp.y+oy},HUDStyle::YELLOW,buf,false); oy+=HUDStyle::FONT_SMALL*sc+3.f; }
        if (sG && (int)m_history.size()>1) {
            float maxV=*std::max_element(m_history.begin(),m_history.end());
            if (maxV<0.01f) maxV=0.01f;
            dl->AddRectFilled({bx,bp.y+oy},{bx+gw,bp.y+oy+gh},HUDStyle::BAR_BG,3.f);
            int n=(int)m_history.size(); float sw=gw/(n-1);
            for (int i=1;i<n;i++) {
                float x1=bx+(i-1)*sw,x2=bx+i*sw;
                float y1=bp.y+oy+gh-(m_history[i-1]/maxV)*gh;
                float y2=bp.y+oy+gh-(m_history[i  ]/maxV)*gh;
                dl->AddLine({x1,y1},{x2,y2},col,1.4f);
            }
            if (sM&&m_maxBps>0.f) {
                float ry=bp.y+oy+gh-(m_maxBps/maxV)*gh;
                ry=ry<bp.y+oy?bp.y+oy:ry>bp.y+oy+gh?bp.y+oy+gh:ry;
                dl->AddLine({bx,ry},{bx+gw,ry},IM_COL32(255,200,40,40),1.f);
            }
        }
    }
    ImGui::End();
    HUDStyle::pop();
}
