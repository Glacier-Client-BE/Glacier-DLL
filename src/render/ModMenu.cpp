#include "ModMenu.h"
#include "../modules/ModuleManager.h"
#include "../utils/ClientConfig.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <IconsFontAwesome5.h>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <cstdio>

// ── Palette constants ─────────────────────────────────────────────────────────
static constexpr ImVec4 kv4Blurple { 0.447f, 0.537f, 0.855f, 1.f };
static constexpr ImVec4 kv4White   { 1.f,    1.f,    1.f,    1.f };
static constexpr ImVec4 kv4Grey    { 0.600f, 0.667f, 0.710f, 1.f };
static constexpr ImVec4 kv4Dark1   { 0.137f, 0.153f, 0.165f, 1.f };
static constexpr ImVec4 kv4Dark2   { 0.172f, 0.184f, 0.200f, 1.f };

static constexpr ImU32 kBlurple  = IM_COL32(114, 137, 218, 255);
static constexpr ImU32 kDark1    = IM_COL32( 35,  39,  42, 255);
static constexpr ImU32 kDark2    = IM_COL32( 44,  47,  51, 255);
static constexpr ImU32 kDark3    = IM_COL32( 30,  33,  36, 255);
static constexpr ImU32 kBorder   = IM_COL32(114, 137, 218,  55);
static constexpr ImU32 kWhite    = IM_COL32(255, 255, 255, 255);
static constexpr ImU32 kGrey     = IM_COL32(153, 170, 181, 210);
static constexpr ImU32 kGreenOn  = IM_COL32( 67, 181, 129, 255);

// ── Tab descriptors ───────────────────────────────────────────────────────────
struct TabDesc {
    const char*    icon;
    const char*    label;
    ModuleCategory cat;
    bool           isSettings;
};

static constexpr TabDesc kTabs[] = {
    { ICON_FA_DESKTOP,      "HUD",      ModuleCategory::HUD,      false },
    { ICON_FA_FIST_RAISED,  "Combat",   ModuleCategory::Combat,   false },
    { ICON_FA_RUNNING,      "Movement", ModuleCategory::Movement,  false },
    { ICON_FA_EYE,          "Visual",   ModuleCategory::Visual,   false },
    { ICON_FA_WRENCH,       "Utility",  ModuleCategory::Utility,  false },
    { ICON_FA_COG,          "Client",   ModuleCategory::Utility,  true  },
};
static constexpr int kTabCount = 6;

// ── Helpers ───────────────────────────────────────────────────────────────────
static bool containsCI(const std::string& hay, const char* needle) {
    if (!needle || !needle[0]) return true;
    std::string h = hay, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

static const char* vkName(int vk) {
    static char buf[32];
    if (vk >= 'A' && vk <= 'Z') { buf[0] = static_cast<char>(vk); buf[1] = '\0'; return buf; }
    if (vk >= '0' && vk <= '9') { buf[0] = static_cast<char>(vk); buf[1] = '\0'; return buf; }
    if (vk >= VK_F1 && vk <= VK_F12) {
        snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1);
        return buf;
    }
    switch (vk) {
        case VK_INSERT: return "INS";
        case VK_DELETE: return "DEL";
        case VK_HOME:   return "HOME";
        case VK_END:    return "END";
        case VK_PRIOR:  return "PGUP";
        case VK_NEXT:   return "PGDN";
        case VK_SPACE:  return "SPC";
        case VK_RETURN: return "ENTER";
        default: snprintf(buf, sizeof(buf), "0x%02X", vk); return buf;
    }
}

// ── Singleton ─────────────────────────────────────────────────────────────────
ModMenu& ModMenu::get() {
    static ModMenu instance;
    return instance;
}

void ModMenu::init() {}

// ─────────────────────────────────────────────────────────────────────────────
//  Main render
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::render() {
    if (!m_open) return;

    // If capturing a key, grab the first pressed key and assign it
    if (m_capturingKey) {
        for (int vk = 0x08; vk <= 0xFE; ++vk) {
            if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
            if (GetAsyncKeyState(vk) & 0x8001) {
                ClientConfig::get().menuKey = vk;
                m_capturingKey = false;
                break;
            }
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        { (io.DisplaySize.x - kMenuW) * 0.5f, (io.DisplaySize.y - kMenuH) * 0.5f },
        ImGuiCond_Once);
    ImGui::SetNextWindowSize({ kMenuW, kMenuH }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   { 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse     |
        ImGuiWindowFlags_NoScrollbar    |
        ImGuiWindowFlags_NoResize       |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##glacier_menu", &m_open, flags)) {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetWindowPos();
        ImVec2      sz  = ImGui::GetWindowSize();

        // ── Drop shadow ───────────────────────────────────────────────────
        for (int i = 4; i >= 1; --i) {
            float f = static_cast<float>(i);
            dl->AddRectFilled(
                { pos.x + f, pos.y + f }, { pos.x + sz.x + f, pos.y + sz.y + f },
                IM_COL32(0, 0, 0, 25), 14.f);
        }

        // ── Window background ─────────────────────────────────────────────
        dl->AddRectFilled(pos, { pos.x + sz.x, pos.y + sz.y }, kDark2, 12.f);
        dl->AddRect(pos,       { pos.x + sz.x, pos.y + sz.y }, kBorder, 12.f, 0, 1.f);

        // ── Sidebar fill ──────────────────────────────────────────────────
        dl->AddRectFilled(pos, { pos.x + kSidebarW, pos.y + sz.y }, kDark1, 12.f,
                          ImDrawFlags_RoundCornersLeft);

        // ── Title bar ─────────────────────────────────────────────────────
        dl->AddRectFilled(pos, { pos.x + sz.x, pos.y + 46.f }, kDark3, 12.f,
                          ImDrawFlags_RoundCornersTop);
        dl->AddLine({ pos.x, pos.y + 46.f }, { pos.x + sz.x, pos.y + 46.f }, kBorder);

        // ── Title text ────────────────────────────────────────────────────
        ImGui::SetCursorPos({ 14.f, 13.f });
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Blurple);
        ImGui::Text(ICON_FA_SNOWFLAKE "  Glacier Client");
        ImGui::PopStyleColor();

        ImGui::SameLine(0.f, 8.f);
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::Text(GLACIER_VERSION);
        ImGui::PopStyleColor();

        // ── Search bar ────────────────────────────────────────────────────
        bool isSettingsTab = kTabs[m_activeTab].isSettings;
        if (!isSettingsTab) {
            float searchW = 190.f;
            ImGui::SameLine(sz.x - searchW - kSettingsW - 44.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg,        kv4Dark1);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kv4Dark1);
            ImGui::PushStyleColor(ImGuiCol_Text,           kv4White);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
            ImGui::SetNextItemWidth(searchW);
            ImGui::InputTextWithHint("##search",
                ICON_FA_SEARCH "  Search modules...",
                m_searchBuf, sizeof(m_searchBuf));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }

        // ── Close button ──────────────────────────────────────────────────
        ImGui::SameLine(sz.x - 36.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.2f,0.2f,0.7f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.8f,0.2f,0.2f,1.f  });
        ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
        if (ImGui::Button(ICON_FA_TIMES "##close", { 28.f, 24.f }))
            m_open = false;
        ImGui::PopStyleColor(4);

        // ── Layout: sidebar | module list | (settings panel) ─────────────
        ImGui::SetCursorPos({ 0.f, 46.f });
        ImGui::BeginGroup();

        renderSidebar();
        ImGui::SameLine(0.f, 0.f);

        if (isSettingsTab) {
            renderClientSettings();
        } else {
            renderModuleList();
            if (m_selectedModule) {
                ImGui::SameLine(0.f, 0.f);
                renderSettingsPanel(m_selectedModule);
            }
        }

        ImGui::EndGroup();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sidebar
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderSidebar() {
    float contentH = kMenuH - 46.f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
    ImGui::BeginChild("##sidebar", { kSidebarW, contentH }, false,
                      ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl  = ImGui::GetWindowDrawList();

    for (int i = 0; i < kTabCount; i++) {
        bool active  = (m_activeTab == i);
        ImVec2 tabMin = ImGui::GetCursorScreenPos();
        ImVec2 tabMax = { tabMin.x + kSidebarW, tabMin.y + 50.f };

        if (active) {
            dl->AddRectFilled(tabMin, tabMax, IM_COL32(114, 137, 218, 35));
            dl->AddRectFilled(
                { tabMin.x, tabMin.y + 6.f },
                { tabMin.x + 3.f, tabMax.y - 6.f },
                kBlurple, 2.f);
        }

        // Icon (large, centered above text)
        ImVec2 iconPos {
            tabMin.x + (kSidebarW - ImGui::CalcTextSize(kTabs[i].icon).x) * 0.5f,
            tabMin.y + 6.f
        };
        dl->AddText(iconPos,
                    active ? kBlurple : IM_COL32(153, 170, 181, 160),
                    kTabs[i].icon);

        // Label
        ImVec2 lblSz  = ImGui::CalcTextSize(kTabs[i].label);
        ImVec2 lblPos {
            tabMin.x + (kSidebarW - lblSz.x) * 0.5f,
            tabMin.y + 22.f
        };
        dl->AddText(lblPos,
                    active ? kWhite : IM_COL32(153, 170, 181, 160),
                    kTabs[i].label);

        // Module count badge (skip for Client Settings tab)
        if (!kTabs[i].isSettings) {
            int count = 0;
            for (auto& mp : ModuleManager::get().getModules())
                if (mp->getCategory() == kTabs[i].cat) count++;
            char cnt[8]; snprintf(cnt, sizeof(cnt), "%d", count);
            ImVec2 cts = ImGui::CalcTextSize(cnt);
            float bx = tabMax.x - cts.x - 8.f, by = tabMin.y + 6.f;
            dl->AddRectFilled({bx-3.f,by-2.f},{bx+cts.x+3.f,by+cts.y+2.f},
                              active ? IM_COL32(114,137,218,60) : IM_COL32(44,47,51,180), 3.f);
            dl->AddText({bx,by}, active ? kBlurple : IM_COL32(100,110,130,180), cnt);
        }

        // Invisible click target
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.447f,0.537f,0.855f,0.12f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.447f,0.537f,0.855f,0.25f });

        char bid[16];
        snprintf(bid, sizeof(bid), "##tab%d", i);
        if (ImGui::Button(bid, { kSidebarW, 50.f })) {
            m_activeTab      = i;
            m_selectedModule = nullptr;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Module list
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderModuleList() {
    bool  searching = m_searchBuf[0] != '\0';
    float listW     = (kMenuW - kSidebarW) - (m_selectedModule ? kSettingsW : 0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 10.f, 10.f });
    ImGui::BeginChild("##modlist", { listW, kMenuH - 46.f }, false);

    const ModuleCategory activeCat = kTabs[m_activeTab].cat;
    auto& mods = ModuleManager::get().getModules();

    int  visCount = 0;
    for (auto& modPtr : mods) {
        ModuleBase* mod = modPtr.get();

        bool matchesCat    = searching || mod->getCategory() == activeCat;
        bool matchesSearch = containsCI(mod->getName(), m_searchBuf) ||
                             containsCI(mod->getDescription(), m_searchBuf);
        if (!matchesCat || !matchesSearch) continue;
        visCount++;

        bool enabled  = mod->isEnabled();
        bool selected = (m_selectedModule == mod);

        // ── Card geometry ─────────────────────────────────────────────────
        ImVec2 cardMin = ImGui::GetCursorScreenPos();
        float  cardW   = listW - 22.f;
        float  cardH   = 64.f;
        ImVec2 cardMax = { cardMin.x + cardW, cardMin.y + cardH };

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Background
        ImU32 cardBg     = selected ? IM_COL32(114,137,218,28) : kDark1;
        ImU32 cardBorder = (selected || enabled)
            ? IM_COL32(114,137,218, selected ? 170 : 55)
            : IM_COL32(55, 60, 66, 180);

        dl->AddRectFilled(cardMin, cardMax, cardBg, 8.f);
        dl->AddRect(cardMin, cardMax, cardBorder, 8.f, 0, 1.5f);

        // Left accent bar when enabled
        if (enabled)
            dl->AddRectFilled(
                { cardMin.x + 1.f, cardMin.y + 10.f },
                { cardMin.x + 4.f, cardMax.y - 10.f },
                kBlurple, 2.f);

        // ── Icon badge ────────────────────────────────────────────────────
        float iconBadgeX = cardMin.x + 12.f;
        float iconBadgeY = cardMin.y + (cardH - 28.f) * 0.5f;
        dl->AddRectFilled({ iconBadgeX, iconBadgeY },
                          { iconBadgeX + 28.f, iconBadgeY + 28.f },
                          IM_COL32(114,137,218, enabled ? 50 : 25), 6.f);
        if (!mod->getIcon().empty()) {
            ImVec2 its = ImGui::CalcTextSize(mod->getIcon().c_str());
            dl->AddText({ iconBadgeX + (28.f - its.x) * 0.5f,
                          iconBadgeY + (28.f - its.y) * 0.5f },
                        enabled ? kBlurple : IM_COL32(153,170,181,160),
                        mod->getIcon().c_str());
        }

        // ── Module name ───────────────────────────────────────────────────
        dl->AddText(nullptr, 13.f,
                    { cardMin.x + 48.f, cardMin.y + 11.f },
                    kWhite, mod->getName().c_str());

        // ── Description ───────────────────────────────────────────────────
        dl->AddText(nullptr, 11.f,
                    { cardMin.x + 48.f, cardMin.y + 28.f },
                    kGrey, mod->getDescription().c_str());

        // ── Keybind chip ──────────────────────────────────────────────────
        const char* kb  = vkName(mod->getKey());
        ImVec2      kbs = ImGui::CalcTextSize(kb);
        float       kbx = cardMax.x - kbs.x - 60.f;
        float       kby = cardMin.y + 11.f;
        dl->AddRectFilled({ kbx - 4.f, kby - 2.f },
                          { kbx + kbs.x + 4.f, kby + kbs.y + 2.f },
                          kDark2, 4.f);
        dl->AddText(nullptr, 11.f, { kbx, kby }, kGrey, kb);

        // ── Toggle switch ─────────────────────────────────────────────────
        float swX = cardMax.x - 46.f;
        float swY = cardMin.y + (cardH - 16.f) * 0.5f;
        float swW = 36.f, swH = 16.f, swR = swH * 0.5f;

        ImU32 swBg = enabled ? kGreenOn : IM_COL32(60,64,70,255);
        dl->AddRectFilled({ swX, swY }, { swX + swW, swY + swH }, swBg, swR);
        float knobX = enabled ? swX + swW - swR - 1.5f : swX + swR + 1.5f;
        dl->AddCircleFilled({ knobX, swY + swR }, swR - 2.f, kWhite);

        // ── Settings gear dot hint ─────────────────────────────────────────
        dl->AddText(nullptr, 10.f,
                    { cardMax.x - 46.f, cardMin.y + 44.f },
                    IM_COL32(114,137,218, selected ? 200 : 80),
                    ICON_FA_COG);

        // ── Invisible hit target ──────────────────────────────────────────
        ImGui::SetCursorScreenPos(cardMin);
        char btnId[80];
        snprintf(btnId, sizeof(btnId), "##mc_%s", mod->getName().c_str());
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.03f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.07f });

        if (ImGui::Button(btnId, { cardW, cardH }))
            mod->toggle();

        ImGui::PopStyleColor(3);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            m_selectedModule = selected ? nullptr : mod;

        // Tooltip
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Left click: toggle\nRight click: settings");

        ImGui::SetCursorScreenPos({ cardMin.x, cardMax.y + 5.f });
    }

    if (visCount == 0) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.f);
        float tw = ImGui::CalcTextSize("No modules found.").x;
        ImGui::SetCursorPosX((listW - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::TextUnformatted("No modules found.");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Per-module settings panel (right side)
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderSettingsPanel(ModuleBase* mod) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 12.f, 14.f });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kv4Dark1);

    ImGui::BeginChild("##settings", { kSettingsW, kMenuH - 46.f }, false);

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();

    // Header
    float hdrY = pos.y + 14.f;
    if (!mod->getIcon().empty())
        dl->AddText(nullptr, 15.f, { pos.x + 12.f, hdrY }, kBlurple,
                    mod->getIcon().c_str());

    dl->AddText(nullptr, 11.f,
                { pos.x + (mod->getIcon().empty() ? 12.f : 30.f), hdrY + 1.f },
                kBlurple, "SETTINGS");
    dl->AddText(nullptr, 12.f, { pos.x + 12.f, hdrY + 16.f },
                kWhite, mod->getName().c_str());
    dl->AddLine({ pos.x + 8.f, pos.y + 50.f },
                { pos.x + kSettingsW - 8.f, pos.y + 50.f }, kBorder);

    // X button
    ImGui::SetCursorPos({ kSettingsW - 28.f, 10.f });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.2f,0.2f,0.5f });
    ImGui::PushStyleColor(ImGuiCol_Text,          kv4Grey);
    if (ImGui::Button(ICON_FA_TIMES "##closesettings", { 20.f, 20.f }))
        m_selectedModule = nullptr;
    ImGui::PopStyleColor(3);

    ImGui::SetCursorPosY(56.f);

    auto& defs = mod->settings().getDefs();
    if (defs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::TextWrapped("No configurable settings.");
        ImGui::PopStyleColor();
    } else {
        for (auto& [key, def] : defs)
            renderSettingWidget(key, const_cast<SettingDef&>(def));
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Client Settings tab
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderClientSettings() {
    float panelW = kMenuW - kSidebarW;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 18.f, 18.f });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kv4Dark2);
    ImGui::BeginChild("##clientsettings", { panelW, kMenuH - 46.f }, false);

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();

    // ── Section: Menu Keybind ─────────────────────────────────────────────
    dl->AddText(nullptr, 11.f, { pos.x + 18.f, pos.y + 18.f },
                kBlurple, "MENU KEYBIND");
    dl->AddLine({ pos.x + 18.f, pos.y + 32.f },
                { pos.x + panelW - 18.f, pos.y + 32.f }, kBorder);

    ImGui::SetCursorPosY(40.f);

    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextWrapped("The key used to open and close this menu.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Current key display
    ImVec2 kboxMin = ImGui::GetCursorScreenPos();
    ImVec2 kboxMax = { kboxMin.x + panelW - 36.f, kboxMin.y + 34.f };

    ImU32 kboxBg = m_capturingKey
        ? IM_COL32(114,137,218, 40)
        : kDark1;
    ImU32 kboxBorder = m_capturingKey
        ? kBlurple
        : IM_COL32(60,65,72,255);

    dl->AddRectFilled(kboxMin, kboxMax, kboxBg, 6.f);
    dl->AddRect(kboxMin, kboxMax, kboxBorder, 6.f, 0,
                m_capturingKey ? 1.5f : 1.f);

    const char* label = m_capturingKey
        ? ICON_FA_KEYBOARD "  Press any key..."
        : vkName(ClientConfig::get().menuKey);

    ImVec2 ls = ImGui::CalcTextSize(label);
    dl->AddText({ kboxMin.x + ((kboxMax.x - kboxMin.x) - ls.x) * 0.5f,
                  kboxMin.y + ((kboxMax.y - kboxMin.y) - ls.y) * 0.5f },
                m_capturingKey ? kBlurple : kWhite, label);

    ImGui::SetCursorScreenPos(kboxMin);
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.04f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.08f });
    if (ImGui::Button("##keybind_btn", { panelW - 36.f, 34.f }))
        m_capturingKey = !m_capturingKey;
    ImGui::PopStyleColor(3);

    ImGui::SetCursorScreenPos({ kboxMin.x, kboxMax.y + 8.f });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextWrapped("Click the box, then press the desired key.");
    ImGui::PopStyleColor();

    // ── Section: Client Info ──────────────────────────────────────────────
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.f);

    ImVec2 secPos = ImGui::GetCursorScreenPos();
    dl->AddText(nullptr, 11.f, { secPos.x, secPos.y },
                kBlurple, "CLIENT INFO");
    dl->AddLine({ secPos.x, secPos.y + 14.f },
                { secPos.x + panelW - 36.f, secPos.y + 14.f }, kBorder);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 22.f);

    auto infoRow = [&](const char* lbl, const char* val) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::Text("%-14s", lbl);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.f, 6.f);
        ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
        ImGui::TextUnformatted(val);
        ImGui::PopStyleColor();
    };

    infoRow("Name:",    "Glacier Client");
    infoRow("Version:", GLACIER_VERSION);
    infoRow("Target:",  "Minecraft Bedrock v26.x");
    infoRow("Render:",  "DirectX 11 + ImGui");

    // ── Section: Keybind reference ────────────────────────────────────────
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.f);

    ImVec2 krefPos = ImGui::GetCursorScreenPos();
    dl->AddText(nullptr, 11.f, { krefPos.x, krefPos.y },
                kBlurple, "DEFAULT KEYBINDS");
    dl->AddLine({ krefPos.x, krefPos.y + 14.f },
                { krefPos.x + panelW - 36.f, krefPos.y + 14.f }, kBorder);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 22.f);

    auto& mods = ModuleManager::get().getModules();
    for (auto& m : mods) {
        char keyStr[16];
        snprintf(keyStr, sizeof(keyStr), "F%d", m->getKey() - VK_F1 + 1);
        infoRow(keyStr, m->getName().c_str());
    }
    infoRow("END", "Unload client");

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Per-module setting widget
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderSettingWidget(const std::string& key, SettingDef& def) {
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextUnformatted(def.label.c_str());
    ImGui::PopStyleColor();

    float w = kSettingsW - 28.f;
    ImGui::SetNextItemWidth(w);

    char wid[128];
    snprintf(wid, sizeof(wid), "##s_%s", key.c_str());

    if (std::holds_alternative<bool>(def.value)) {
        bool v = std::get<bool>(def.value);
        if (ImGui::Checkbox(wid, &v)) def.value = v;

    } else if (std::holds_alternative<int>(def.value)) {
        int v  = std::get<int>(def.value);
        int mn = std::get<int>(def.minVal);
        int mx = std::get<int>(def.maxVal);
        if (ImGui::SliderInt(wid, &v, mn, mx)) def.value = v;

    } else if (std::holds_alternative<float>(def.value)) {
        float v  = std::get<float>(def.value);
        float mn = std::get<float>(def.minVal);
        float mx = std::get<float>(def.maxVal);
        if (ImGui::SliderFloat(wid, &v, mn, mx, "%.1f")) def.value = v;

    } else if (std::holds_alternative<std::string>(def.value)) {
        auto& sv = std::get<std::string>(def.value);
        char  buf[256]{};
        strncpy_s(buf, sv.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText(wid, buf, sizeof(buf))) sv = buf;
    }

    ImGui::Spacing();
}
