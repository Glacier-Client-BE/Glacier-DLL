// ─ ActiveModsList ─────────────────────────────────────────────────────────────
#include "ActiveModsList.h"
#include "../ModuleManager.h"
#include "../../render/HUDStyle.h"
#include <imgui.h>
#include <algorithm>

ActiveModsList::ActiveModsList()
    : ModuleBase("Active Mods","Enabled modules list",
                 "activemodslist", ModuleCategory::HUD, 10.f, 640.f)
{
    m_settings.defineBool ("sorted",  "Alphabetical",   true);
    m_settings.defineFloat("scale",   "Scale",          1.f, 0.5f, 2.f);
    m_settings.defineBool ("rightAlign","Right-Align",  false);
}
void ActiveModsList::onRenderImGui() {
    auto& mods=ModuleManager::get().getModules();
    std::vector<std::string> names;
    for (auto& m : mods) if (m->isEnabled()&&m.get()!=this) names.push_back(m->getName());
    if (names.empty()) return;
    if (m_settings.getBool("sorted")) std::sort(names.begin(),names.end());
    float sc  = m_settings.getFloat("scale");
    float fs  = HUDStyle::FONT_MID * sc;
    float lh  = fs + 4.f;
    bool  rA  = m_settings.getBool("rightAlign");
    float maxW=60.f;
    for (auto& n : names) maxW=std::max(maxW, ImGui::CalcTextSize(n.c_str()).x * fs/ImGui::GetFontSize());
    float panW=maxW+HUDStyle::PAD_X*2+6.f;
    float panH=(float)names.size()*lh+HUDStyle::PAD_Y*2;
    ImGui::SetNextWindowPos(m_pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize({panW,panH});
    HUDStyle::push();
    if (ImGui::Begin("##aml",nullptr,HUDStyle::WIN_FLAGS)) {
        HUDStyle::drag(m_pos);
        ImDrawList* dl=ImGui::GetWindowDrawList();
        ImVec2 base=ImGui::GetWindowPos();
        base.x+=HUDStyle::PAD_X; base.y+=HUDStyle::PAD_Y;
        for (int i=0;i<(int)names.size();i++) {
            float tw=ImGui::CalcTextSize(names[i].c_str()).x*fs/ImGui::GetFontSize();
            float tx=rA?base.x+maxW-tw:base.x;
            float oy=(float)i*lh;
            if (!rA) dl->AddRectFilled({base.x-5.f,base.y+oy+2.f},{base.x-3.f,base.y+oy+fs-2.f},HUDStyle::ACCENT,1.f);
            HUDStyle::text(dl,fs,{tx,base.y+oy},HUDStyle::WHITE,names[i].c_str());
        }
    }
    ImGui::End(); HUDStyle::pop();
}
