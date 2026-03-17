#include "PacketLogger.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
PacketLogger::PacketLogger()
    : ModuleBase("Packet Logger","Logs incoming/outgoing packets",ICON_FA_NETWORK_WIRED,ModuleCategory::Utility)
{
    m_settings.defineInt  ("maxLines",  "Max Lines",      64, 8,256);
    m_settings.defineBool ("showInbound","Log Inbound",   true);
    m_settings.defineBool ("showOut",    "Log Outbound",  true);
    m_settings.defineFloat("fontSize",   "Font Size",    11.f,8.f,18.f);
    m_settings.defineFloat("panelW",     "Panel Width", 340.f,200.f,700.f);
    m_settings.defineFloat("panelH",     "Panel Height",160.f,80.f,400.f);
}
void PacketLogger::log(const std::string& pkt){ s_log.push_back(pkt); if(s_log.size()>256)s_log.pop_front(); }
void PacketLogger::onRenderImGui(){
    float pw=m_settings.getFloat("panelW"), ph=m_settings.getFloat("panelH");
    float fs=m_settings.getFloat("fontSize"); int maxL=m_settings.getInt("maxLines");
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Once); ImGui::SetNextWindowSize({pw,ph},ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.90f);
    ImGuiWindowFlags f=ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.137f,0.153f,0.165f,0.9f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,ImVec4(0.137f,0.153f,0.165f,1.f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,ImVec4(0.172f,0.184f,0.2f,1.f));
    if(ImGui::Begin(ICON_FA_NETWORK_WIRED " Packet Log##pklog",nullptr,f)){
        if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){auto d=ImGui::GetIO().MouseDelta;m_pos.x+=d.x;m_pos.y+=d.y;}
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{2,1});
        int start=(int)s_log.size()>maxL?(int)s_log.size()-maxL:0;
        for(int i=start;i<(int)s_log.size();i++){
            ImGui::SetWindowFontScale(fs/13.f);
            ImGui::TextUnformatted(s_log[i].c_str());
        }
        ImGui::SetWindowFontScale(1.f);
        if(ImGui::GetScrollY()>=ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.f);
        ImGui::PopStyleVar();
    }
    ImGui::End(); ImGui::PopStyleColor(3);
}
