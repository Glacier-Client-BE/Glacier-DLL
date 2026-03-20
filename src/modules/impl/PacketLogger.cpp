#include "PacketLogger.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>
#include <chrono>
#include <ctime>

PacketLogger::PacketLogger()
    : ModuleBase("Packet Logger", "Logs incoming/outgoing packets with timestamps",
                 "packetlogger", ModuleCategory::Utility)
{
    m_settings.defineInt  ("maxLines",    "Max Lines",      100,   8,  512);
    m_settings.defineBool ("showIn",      "Log Inbound",    true);
    m_settings.defineBool ("showOut",     "Log Outbound",   true);
    m_settings.defineBool ("timestamps",  "Show Timestamps",true);
    m_settings.defineBool ("autoScroll",  "Auto Scroll",    true);
    m_settings.defineFloat("fontSize",    "Font Size",      11.f, 8.f, 18.f);
    m_settings.defineFloat("panelW",      "Panel Width",   360.f,200.f,700.f);
    m_settings.defineFloat("panelH",      "Panel Height",  180.f, 80.f,400.f);
}

void PacketLogger::log(const std::string& pkt, bool inbound) {
    if (inbound  && !m_settings.getBool("showIn"))  return;
    if (!inbound && !m_settings.getBool("showOut")) return;

    char ts[12] = {};
    if (m_settings.getBool("timestamps")) {
        time_t t = time(nullptr);
        tm* lt   = localtime(&t);
        snprintf(ts, sizeof(ts), "[%02d:%02d:%02d] ", lt->tm_hour, lt->tm_min, lt->tm_sec);
    }

    std::string entry = std::string(ts) + (inbound ? "[IN]  " : "[OUT] ") + pkt;
    m_log.push_back({ entry, inbound });

    int maxL = m_settings.getInt("maxLines");
    while ((int)m_log.size() > maxL) m_log.pop_front();
}

void PacketLogger::onEnable()  { m_log.clear(); }

void PacketLogger::onRenderImGui() {
    float pw = m_settings.getFloat("panelW");
    float ph = m_settings.getFloat("panelH");
    float fs = m_settings.getFloat("fontSize");

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ pw, ph }, ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,       ImVec4(0.118f, 0.129f, 0.141f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,        ImVec4(0.118f, 0.129f, 0.141f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,  ImVec4(0.172f, 0.184f, 0.200f, 1.f));

    if (ImGui::Begin("packetlogger" " Packet Log##pklog", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta; m_pos.x += d.x; m_pos.y += d.y;
        }
        // Clear button
        ImGui::SameLine(pw - 60.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.447f,0.537f,0.855f,0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.447f,0.537f,0.855f,0.6f));
        if (ImGui::SmallButton("Clear")) m_log.clear();
        ImGui::PopStyleColor(2);

        ImGui::Separator();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2.f, 1.f });
        ImGui::SetWindowFontScale(fs / 13.f);

        for (auto& entry : m_log) {
            ImVec4 col = entry.inbound
                ? ImVec4(0.72f, 0.87f, 0.63f, 1.f)
                : ImVec4(0.86f, 0.65f, 0.45f, 1.f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(entry.text.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::SetWindowFontScale(1.f);
        if (m_settings.getBool("autoScroll") &&
            ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.f)
            ImGui::SetScrollHereY(1.f);
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleColor(3);
}
