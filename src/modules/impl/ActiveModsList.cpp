#include "ActiveModsList.h"
#include "../ModuleManager.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <algorithm>
#include <vector>
#include <string>

ActiveModsList::ActiveModsList()
    : ModuleBase("Active Mods List","Shows enabled modules on screen",
                 ICON_FA_LIST, ModuleCategory::HUD, 10.f, 640.f)
{
    m_settings.defineFloat("fontSize",  "Font Size",       12.f, 8.f, 24.f);
    m_settings.defineBool ("shadow",    "Shadow",           true);
    m_settings.defineBool ("showIcons", "Show Icons",       true);
    m_settings.defineBool ("sorted",    "Alphabetical",     true);
    m_settings.defineBool ("bgBox",     "Background Box",  false);
    m_settings.defineFloat("bgAlpha",   "BG Alpha",         0.5f, 0.f, 1.f);
    m_settings.defineFloat("spacing",   "Line Spacing",     2.f,  0.f, 8.f);
    // Accent bar colour
    m_settings.defineFloat("accentR",   "Accent R",        114.f, 0.f, 255.f);
    m_settings.defineFloat("accentG",   "Accent G",        137.f, 0.f, 255.f);
    m_settings.defineFloat("accentB",   "Accent B",        218.f, 0.f, 255.f);
}

void ActiveModsList::onRenderImGui() {
    auto& mods = ModuleManager::get().getModules();

    // Collect enabled modules (skip self)
    std::vector<std::string> names;
    std::vector<std::string> icons;
    for (auto& m : mods) {
        if (!m->isEnabled() || m.get() == this) continue;
        names.push_back(m->getName());
        icons.push_back(m->getIcon());
    }
    if (names.empty()) return;

    if (m_settings.getBool("sorted")) {
        // Sort alphabetically (carry icons along)
        std::vector<int> idx(names.size());
        for (int i = 0; i < (int)idx.size(); i++) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [&](int a, int b){
            return names[a] < names[b];
        });
        std::vector<std::string> sn, si;
        for (int i : idx) { sn.push_back(names[i]); si.push_back(icons[i]); }
        names = sn; icons = si;
    }

    float fs      = m_settings.getFloat("fontSize");
    float spacing = m_settings.getFloat("spacing");
    float lineH   = fs + spacing;
    bool  shad    = m_settings.getBool("shadow");
    bool  showIco = m_settings.getBool("showIcons");
    bool  bg      = m_settings.getBool("bgBox");
    float bgA     = m_settings.getFloat("bgAlpha");

    ImU32 accent = IM_COL32((int)m_settings.getFloat("accentR"),
                             (int)m_settings.getFloat("accentG"),
                             (int)m_settings.getFloat("accentB"), 255);

    // Measure widest entry for background box
    float maxW = 80.f;
    for (int i = 0; i < (int)names.size(); i++) {
        std::string label = showIco && !icons[i].empty()
            ? (icons[i] + "  " + names[i])
            : names[i];
        float w = ImGui::CalcTextSize(label.c_str()).x;
        if (w > maxW) maxW = w;
    }
    float panW = maxW + 16.f;
    float panH = (float)names.size() * lineH + 8.f;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW, panH});
    ImGui::SetNextWindowBgAlpha(bg ? bgA : 0.f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.137f,0.153f,0.165f, bg ? bgA : 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   {6.f, 4.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    if (ImGui::Begin("##activemods", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta; m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      base= ImGui::GetWindowPos();
        base.x += 6.f; base.y += 4.f;

        for (int i = 0; i < (int)names.size(); i++) {
            float oy = (float)i * lineH;

            // Left accent bar
            dl->AddRectFilled({base.x - 4.f, base.y + oy},
                              {base.x - 2.f, base.y + oy + fs},
                              accent, 1.f);

            std::string label = showIco && !icons[i].empty()
                ? (icons[i] + "  " + names[i])
                : names[i];

            if (shad)
                dl->AddText(nullptr, fs,
                            {base.x+1, base.y+oy+1}, IM_COL32(0,0,0,160), label.c_str());
            dl->AddText(nullptr, fs,
                        {base.x, base.y+oy}, IM_COL32(255,255,255,230), label.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
