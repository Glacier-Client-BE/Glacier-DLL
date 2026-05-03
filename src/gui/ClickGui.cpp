#include "ClickGui.h"
#include "../core/Glacier.h"
#include "../core/Logger.h"
#include "../events/EventBus.h"
#include "../events/Events.h"
#include "../modules/ModuleManager.h"
#include "../config/Config.h"
#include "../render/Theme.h"
#include "../render/Fonts.h"

#include <imgui.h>
#include <Windows.h>
#include <algorithm>
#include <cctype>

namespace Glacier {

ClickGui& ClickGui::get() {
    static ClickGui G;
    return G;
}

void ClickGui::init() {
    EventBus::get().listen<KeyEvent,        &ClickGui::onKey>(this);
    EventBus::get().listen<RenderImGuiEvent,&ClickGui::onRender>(this);
}

void ClickGui::onKey(KeyEvent& e) {
    if (!e.down) return;
    if (e.vkey == openKey_) {
        open_ = !open_;
        ImGui::GetIO().MouseDrawCursor = open_;
    }
}

static bool icontains(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    return it != hay.end();
}

void ClickGui::onRender(RenderImGuiEvent&) {
    if (!open_) return;

    auto& io = ImGui::GetIO();
    ImVec2 size{ 720, 460 };
    ImGui::SetNextWindowSize(size, ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - size.x) * 0.5f,
                                   (io.DisplaySize.y - size.y) * 0.5f),
                             ImGuiCond_Once);

    ImGui::PushFont(Fonts::get().md());
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::toVec4(COL_BG_BASE));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    char title[64];
    std::snprintf(title, sizeof(title), "%s##glacier_root", kBrand);
    if (ImGui::Begin(title, &open_,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) {

        // ----- Title bar -----
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::toVec4(COL_BG_DEEPEST));
        ImGui::BeginChild("##glacier_titlebar", ImVec2(0, 56), false);
        {
            ImGui::SetCursorPos(ImVec2(20, 16));
            ImGui::PushFont(Fonts::get().lg());
            ImGui::TextColored(Theme::toVec4(COL_ACCENT), "%c", '*');
            ImGui::SameLine(0, 10);
            ImGui::TextUnformatted(kBrand);
            ImGui::SameLine(0, 12);
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::toVec4(COL_TEXT_DIM));
            ImGui::Text("v%s", kVersion);
            ImGui::PopStyleColor();
            ImGui::PopFont();

            // Right-aligned profile selector + close button
            float right = ImGui::GetWindowWidth() - 16.f;
            ImGui::SameLine(right - 30.f);
            if (ImGui::SmallButton("X")) open_ = false;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // ----- Body -----
        ImGui::BeginChild("##glacier_body", ImVec2(0, 0), false);
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 170.f);

        // ---- Left rail: categories ----
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::toVec4(COL_BG_PANEL));
        ImGui::BeginChild("##glacier_rail", ImVec2(0, 0), false);
        {
            ImGui::Dummy(ImVec2(0, 8));
            for (int i = 0; i < kCategoryCount; ++i) {
                Category c = static_cast<Category>(i);
                bool active = (c == activeCategory_);
                ImGui::PushStyleColor(ImGuiCol_Button, Theme::toVec4(active ? COL_ACCENT : 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::toVec4(active ? COL_ACCENT_HOVER : 0xFF323640));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Theme::toVec4(COL_ACCENT_DEEP));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ROUND_SM);

                ImGui::Dummy(ImVec2(8, 0)); ImGui::SameLine(0, 0);
                if (ImGui::Button(std::string(kCategoryName(c)).c_str(), ImVec2(154, 36))) {
                    activeCategory_ = c;
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                ImGui::Dummy(ImVec2(0, 4));
            }

            // Footer: profile + save
            float footerY = ImGui::GetWindowHeight() - 86.f;
            ImGui::SetCursorPosY(footerY);
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::Indent(8.f);
            ImGui::TextColored(Theme::toVec4(COL_TEXT_DIM), "Profile");
            ImGui::PushItemWidth(150);
            char profBuf[64]; std::snprintf(profBuf, sizeof(profBuf), "%s", Config::get().profile().c_str());
            if (ImGui::InputText("##profile", profBuf, sizeof(profBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                Config::get().setProfile(profBuf);
                Config::get().load();
            }
            ImGui::PopItemWidth();
            if (ImGui::Button("Save", ImVec2(72, 26))) Config::get().save();
            ImGui::SameLine();
            if (ImGui::Button("Reload", ImVec2(72, 26))) Config::get().load();
            ImGui::Unindent(8.f);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::NextColumn();

        // ---- Right pane: module cards ----
        ImGui::BeginChild("##glacier_modules", ImVec2(0, 0), false);
        {
            ImGui::Dummy(ImVec2(0, 4));

            // Search
            ImGui::PushItemWidth(-1);
            char fbuf[128]; std::snprintf(fbuf, sizeof(fbuf), "%s", filter_.c_str());
            if (ImGui::InputTextWithHint("##filter", "Search modules...", fbuf, sizeof(fbuf))) {
                filter_ = fbuf;
            }
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(0, 4));

            auto mods = ModuleManager::get().byCategory(activeCategory_);
            for (auto* m : mods) {
                if (!icontains(m->displayName(), filter_) &&
                    !icontains(m->id(),          filter_)) continue;

                ImGui::PushID(m->id().c_str());
                ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::toVec4(COL_BG_PANEL));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ROUND_MD);
                ImGui::BeginChild("##card", ImVec2(0, 0), true,
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
                {
                    // Header line: name + description + toggle
                    bool en = m->enabled();
                    ImGui::Dummy(ImVec2(2, 0)); ImGui::SameLine();
                    ImGui::PushFont(Fonts::get().md());
                    ImGui::TextUnformatted(m->displayName().c_str());
                    ImGui::PopFont();
                    ImGui::SameLine();
                    float right = ImGui::GetWindowContentRegionMax().x - 60.f;
                    ImGui::SetCursorPosX(right);
                    if (ImGui::Checkbox("##en", &en)) m->setEnabled(en);

                    ImGui::PushStyleColor(ImGuiCol_Text, Theme::toVec4(COL_TEXT_DIM));
                    ImGui::TextWrapped("%s", m->description().c_str());
                    ImGui::PopStyleColor();

                    // Settings expander
                    if (!m->settings().empty()) {
                        ImGui::Dummy(ImVec2(0, 2));
                        if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
                            m->drawSettings();
                            ImGui::TreePop();
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                ImGui::PopID();
                ImGui::Dummy(ImVec2(0, 6));
            }
        }
        ImGui::EndChild();
        ImGui::Columns(1);

        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

} // namespace Glacier
