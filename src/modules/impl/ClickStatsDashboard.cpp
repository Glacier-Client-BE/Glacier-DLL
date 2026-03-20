#include "ClickStatsDashboard.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>
#include <numeric>

ClickStatsDashboard::ClickStatsDashboard()
    : ModuleBase("Click Stats","CPS dashboard with peak, average and graph",
                 "clickstats", ModuleCategory::HUD, 200.f, 380.f)
{
    m_settings.defineBool ("showPeak", "Peak CPS",     true);
    m_settings.defineBool ("showAvg",  "Avg CPS",      true);
    m_settings.defineBool ("showTotal","Total Clicks",  true);
    m_settings.defineBool ("showGraph","CPS Graph",     true);
    m_settings.defineFloat("graphH",   "Graph Height", 24.f, 10.f, 50.f);
    m_settings.defineInt  ("maxCPS",   "Graph Max",    20,   5,   50);
    m_settings.defineFloat("scale",    "Scale",        1.f,  0.5f, 2.f);
}
void ClickStatsDashboard::onEnable() {
    m_clicks.clear(); m_cpsHistory.clear();
    m_peakCPS=0.f; m_totalClicks=0; m_pLMB=false; m_accumDt=0.f;
}
void ClickStatsDashboard::onRenderImGui() {
    auto now=Clock::now();
    bool cur=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    if (cur&&!m_pLMB){ m_clicks.push_back(now); m_totalClicks++; }
    m_pLMB=cur;
    auto cut=now-std::chrono::seconds(1);
    while (!m_clicks.empty()&&m_clicks.front()<cut) m_clicks.pop_front();
    float cps=(float)m_clicks.size();
    if (cps>m_peakCPS) m_peakCPS=cps;
    m_accumDt+=ImGui::GetIO().DeltaTime;
    if (m_accumDt>=0.1f){ m_accumDt=0.f; m_cpsHistory.push_back(cps); if((int)m_cpsHistory.size()>80) m_cpsHistory.pop_front(); }
    float avg=m_cpsHistory.empty()?0.f:std::accumulate(m_cpsHistory.begin(),m_cpsHistory.end(),0.f)/(float)m_cpsHistory.size();

    float sc=m_settings.getFloat("scale");
    bool sp=m_settings.getBool("showPeak"),sa=m_settings.getBool("showAvg"),
         st=m_settings.getBool("showTotal"),sg=m_settings.getBool("showGraph");
    float gh=m_settings.getFloat("graphH")*sc;
    int   mx=m_settings.getInt("maxCPS");
    float fs=HUDStyle::FONT_BIG*sc, fss=HUDStyle::FONT_SMALL*sc, lh=fss+3.f;
    float panW=110.f*sc;
    float panH=fs+4.f+(sp?lh:0.f)+(sa?lh:0.f)+(st?lh:0.f)+(sg?gh+6.f:0.f)+HUDStyle::PAD_Y*2;

    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW,panH});
    HUDStyle::push();
    if (ImGui::Begin("##csd",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 bp=ImGui::GetWindowPos();
        float bx=bp.x+HUDStyle::PAD_X, oy=HUDStyle::PAD_Y, bw=panW-HUDStyle::PAD_X*2;
        char buf[32];
        // Big CPS number
        snprintf(buf,sizeof(buf),"%.0f",cps);
        ImU32 cpsCol=cps>mx*0.8f?HUDStyle::RED:cps>0.f?HUDStyle::ACCENT:HUDStyle::GREY;
        HUDStyle::text(dl,fs,{bx,bp.y+oy},cpsCol,buf); oy+=fs+4.f;
        auto row=[&](const char* lbl,const char* val,ImU32 c){
            HUDStyle::text(dl,fss,{bx,bp.y+oy},HUDStyle::GREY,lbl,false);
            ImVec2 vs=ImGui::CalcTextSize(val); float sc2=fss/ImGui::GetFontSize();
            HUDStyle::text(dl,fss,{bx+bw-vs.x*sc2,bp.y+oy},c,val,false); oy+=lh;
        };
        if (sp){ snprintf(buf,sizeof(buf),"%.0f",m_peakCPS); row("Peak:",buf,HUDStyle::YELLOW); }
        if (sa){ snprintf(buf,sizeof(buf),"%.1f",avg);       row("Avg:", buf,HUDStyle::GREEN);  }
        if (st){ snprintf(buf,sizeof(buf),"%d",m_totalClicks);row("Total:",buf,HUDStyle::WHITE); }
        if (sg&&(int)m_cpsHistory.size()>1) {
            float maxV=(float)mx>0.f?(float)mx:1.f;
            dl->AddRectFilled({bx,bp.y+oy},{bx+bw,bp.y+oy+gh},HUDStyle::BAR_BG,3.f);
            int n=(int)m_cpsHistory.size(); float sw=bw/(n-1);
            for (int i=1;i<n;i++){
                float x1=bx+(i-1)*sw,x2=bx+i*sw;
                float y1=bp.y+oy+gh-std::min(m_cpsHistory[i-1]/maxV,1.f)*gh;
                float y2=bp.y+oy+gh-std::min(m_cpsHistory[i  ]/maxV,1.f)*gh;
                dl->AddLine({x1,y1},{x2,y2},HUDStyle::ACCENT,1.2f);
            }
        }
    }
    ImGui::End(); HUDStyle::pop();
}
