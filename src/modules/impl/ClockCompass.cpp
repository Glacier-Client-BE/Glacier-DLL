#include "ClockCompass.h"
#include "../../sdk/ClientInstance.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <ctime>

ClockCompass::ClockCompass()
    : ModuleBase("Clock & Compass","Real-time clock and directional compass",
                 "clockcompass", ModuleCategory::HUD, 10.f, 60.f)
{
    m_settings.defineBool ("showClock",  "Show Clock",   true);
    m_settings.defineBool ("show24h",    "24-Hour",      false);
    m_settings.defineBool ("showCompass","Show Compass", true);
    m_settings.defineBool ("showDeg",    "Show Degrees", false);
    m_settings.defineFloat("scale",      "Scale",        1.f, 0.5f, 2.f);
}
void ClockCompass::onRenderImGui() {
    float sc=m_settings.getFloat("scale");
    bool  sCk=m_settings.getBool("showClock"),sCo=m_settings.getBool("showCompass");
    bool  s24=m_settings.getBool("show24h"),sDeg=m_settings.getBool("showDeg");

    float panW=160.f*sc;
    float panH=(sCk?HUDStyle::FONT_MID*sc+4.f:0.f)+(sCo?28.f*sc+4.f:0.f)+(sDeg?12.f*sc:0.f)+HUDStyle::PAD_Y*2;

    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW,panH});
    HUDStyle::push();
    if (ImGui::Begin("##cc",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 bp=ImGui::GetWindowPos();
        float bx=bp.x+HUDStyle::PAD_X, oy=HUDStyle::PAD_Y;
        float fw=panW-HUDStyle::PAD_X*2;

        if (sCk) {
            time_t t=time(nullptr); struct tm* tm=localtime(&t);
            char buf[32];
            if (s24) snprintf(buf,sizeof(buf),"%02d:%02d:%02d",tm->tm_hour,tm->tm_min,tm->tm_sec);
            else { int h=tm->tm_hour%12; if(!h)h=12;
                snprintf(buf,sizeof(buf),"%d:%02d %s",h,tm->tm_min,tm->tm_hour>=12?"PM":"AM"); }
            float fs=HUDStyle::FONT_MID*sc;
            ImVec2 ts=ImGui::CalcTextSize(buf); float sc3=fs/ImGui::GetFontSize();
            HUDStyle::text(dl,fs,{bx+(fw-ts.x*sc3)*0.5f,bp.y+oy},HUDStyle::WHITE,buf);
            oy+=fs+4.f;
        }

        if (sCo) {
            auto* lp=getLocalPlayer();
            float yaw=lp?lp->getYaw():0.f;
            while(yaw<0.f)yaw+=360.f; while(yaw>=360.f)yaw-=360.f;
            static const char* dirs[8]={"S","SW","W","NW","N","NE","E","SE"};
            static ImU32 dirC[8]={HUDStyle::RED,HUDStyle::GREY,HUDStyle::GREY,HUDStyle::GREY,
                                  HUDStyle::ACCENT,HUDStyle::GREY,HUDStyle::GREY,HUDStyle::GREY};
            float compH=28.f*sc, cy2=bp.y+oy;
            // Draw a thin track line
            dl->AddLine({bx,cy2+compH*0.5f},{bx+fw,cy2+compH*0.5f},IM_COL32(60,65,72,180),1.f);

            for (int i=0;i<8;i++) {
                float ang=(float)i*45.f;
                float diff=ang-yaw;
                while(diff>180.f)diff-=360.f; while(diff<-180.f)diff+=360.f;
                if(fabsf(diff)>100.f) continue;
                float t=diff/100.f;
                float sx=bx+fw*0.5f+t*fw*0.5f;
                float fade=1.f-fabsf(t);
                float fSize=(1.f+(1.f-fabsf(t))*0.4f)*HUDStyle::FONT_MID*sc;
                ImVec2 ts2=ImGui::CalcTextSize(dirs[i]); float sc4=fSize/ImGui::GetFontSize();
                ImU32 c=(dirC[i]&0x00FFFFFF)|((ImU32)((float)((dirC[i]>>24)&0xFF)*fade)<<24);
                if (fabsf(diff)<3.f) {
                    dl->AddRectFilled({sx-ts2.x*sc4*0.5f-4.f,cy2+(compH-fSize)*0.5f-2.f},
                        {sx+ts2.x*sc4*0.5f+4.f,cy2+(compH-fSize)*0.5f+fSize+2.f},
                        IM_COL32(114,137,218,40),4.f);
                }
                dl->AddText(ImGui::GetFont(),fSize,
                    {sx-ts2.x*sc4*0.5f,cy2+(compH-fSize)*0.5f},c,dirs[i]);
            }
            oy+=compH+4.f;

            if (sDeg) {
                char db[16]; snprintf(db,sizeof(db),"%.1f\xc2\xb0",yaw);
                ImVec2 ds=ImGui::CalcTextSize(db); float sc5=HUDStyle::FONT_SMALL*sc/ImGui::GetFontSize();
                HUDStyle::text(dl,HUDStyle::FONT_SMALL*sc,
                    {bx+(fw-ds.x*sc5)*0.5f,bp.y+oy},HUDStyle::GREY,db,false);
            }
        }
    }
    ImGui::End(); HUDStyle::pop();
}
