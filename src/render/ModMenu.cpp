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

static constexpr ImVec4 kv4Blurple { 0.447f, 0.537f, 0.855f, 1.f };
static constexpr ImVec4 kv4White   { 1.f,    1.f,    1.f,    1.f };
static constexpr ImVec4 kv4Grey    { 0.600f, 0.667f, 0.710f, 1.f };
static constexpr ImVec4 kv4Dark1   { 0.137f, 0.153f, 0.165f, 1.f };
static constexpr ImVec4 kv4Dark2   { 0.172f, 0.184f, 0.200f, 1.f };
static constexpr ImVec4 kv4Dark3   { 0.118f, 0.129f, 0.141f, 1.f };

static constexpr ImU32 kBlurple    = IM_COL32(114, 137, 218, 255);
static constexpr ImU32 kBlurpleDim = IM_COL32(114, 137, 218,  80);
static constexpr ImU32 kDark1      = IM_COL32( 35,  39,  42, 255);
static constexpr ImU32 kDark2      = IM_COL32( 44,  47,  51, 255);
static constexpr ImU32 kDark3      = IM_COL32( 30,  33,  36, 255);
static constexpr ImU32 kBorder     = IM_COL32(114, 137, 218,  45);
static constexpr ImU32 kBorderOn   = IM_COL32(114, 137, 218, 160);
static constexpr ImU32 kWhite      = IM_COL32(255, 255, 255, 255);
static constexpr ImU32 kGrey       = IM_COL32(153, 170, 181, 210);
static constexpr ImU32 kGreyDim    = IM_COL32(153, 170, 181, 120);
static constexpr ImU32 kGreenOn    = IM_COL32( 67, 181, 129, 255);

struct TabDesc { const char* icon; const char* label; };
static constexpr TabDesc kTabs[] = {
    { ICON_FA_TH,           "Modules"     },
    { ICON_FA_LAYER_GROUP,  "Elements"    },
    { ICON_FA_PENCIL_RULER, "Editors"     },
    { ICON_FA_INFO_CIRCLE,  "Information" },
};
static constexpr int kTabCount = 4;

struct CatDesc { const char* icon; const char* label; ModuleCategory cat; };
static constexpr CatDesc kCats[] = {
    { ICON_FA_DESKTOP,     "HUD",      ModuleCategory::HUD      },
    { ICON_FA_FIST_RAISED, "Combat",   ModuleCategory::Combat   },
    { ICON_FA_RUNNING,     "Movement", ModuleCategory::Movement },
    { ICON_FA_EYE,         "Visual",   ModuleCategory::Visual   },
    { ICON_FA_WRENCH,      "Utility",  ModuleCategory::Utility  },
};
static constexpr int kCatCount = 5;

static bool containsCI(const std::string& hay, const char* needle) {
    if (!needle || !needle[0]) return true;
    std::string h = hay, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

static const char* vkName(int vk) {
    static char buf[32];
    if (vk == 0) return "\xe2\x80\x94";
    if (vk >= 'A' && vk <= 'Z') { buf[0] = (char)vk; buf[1] = '\0'; return buf; }
    if (vk >= '0' && vk <= '9') { buf[0] = (char)vk; buf[1] = '\0'; return buf; }
    if (vk >= VK_F1 && vk <= VK_F12) { snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1); return buf; }
    switch (vk) {
        case VK_INSERT: return "INS";   case VK_DELETE: return "DEL";
        case VK_HOME:   return "HOME";  case VK_END:    return "END";
        case VK_PRIOR:  return "PGUP";  case VK_NEXT:   return "PGDN";
        case VK_SPACE:  return "SPC";   case VK_RETURN: return "ENTER";
        default: snprintf(buf, sizeof(buf), "0x%02X", vk); return buf;
    }
}

ModMenu& ModMenu::get() { static ModMenu inst; return inst; }
void ModMenu::init() {}

void ModMenu::render() {
    if (!m_open) return;

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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    { 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   14.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("##glacier_menu", &m_open, wf)) {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetWindowPos();
        ImVec2      sz  = ImGui::GetWindowSize();
        ImVec2      wEnd = { pos.x + sz.x, pos.y + sz.y };

        for (int i = 5; i >= 1; --i)
            dl->AddRectFilled({ pos.x + (float)i, pos.y + (float)i },
                { wEnd.x + (float)i, wEnd.y + (float)i }, IM_COL32(0, 0, 0, 18), 16.f);
        dl->AddRectFilled(pos, wEnd, kDark2, 14.f);
        dl->AddRect(pos, wEnd, kBorder, 14.f, 0, 1.2f);

        renderHeader(dl, pos, sz);
        renderTabBar(dl, pos, sz);

        ImGui::SetCursorPos({ 0.f, kHeaderH + kTabBarH });
        switch (m_activeTab) {
            case 0: renderModulesTab(); break;
            case 1: renderElementsTab(); break;
            case 2: renderEditorsTab(); break;
            case 3: renderInformationTab(); break;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void ModMenu::renderHeader(ImDrawList* dl, ImVec2 pos, ImVec2 sz) {
    dl->AddRectFilled(pos, { pos.x + sz.x, pos.y + kHeaderH }, kDark3, 14.f, ImDrawFlags_RoundCornersTop);
    dl->AddLine({ pos.x, pos.y + kHeaderH }, { pos.x + sz.x, pos.y + kHeaderH }, kBorder, 1.f);

    ImGui::SetCursorPos({ 16.f, 11.f });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Blurple);
    ImGui::Text(ICON_FA_SNOWFLAKE "  Glacier");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 8.f);
    ImVec2 vp = ImGui::GetCursorScreenPos();
    ImVec2 vSz = ImGui::CalcTextSize(GLACIER_VERSION);
    dl->AddRectFilled({ vp.x - 4.f, vp.y - 2.f }, { vp.x + vSz.x + 6.f, vp.y + vSz.y + 2.f },
        IM_COL32(114, 137, 218, 28), 4.f);
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextUnformatted(GLACIER_VERSION);
    ImGui::PopStyleColor();

    ImGui::SetCursorPos({ sz.x - 40.f, 7.f });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.2f,0.2f,0.7f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.8f,0.2f,0.2f,1.f  });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::Button(ICON_FA_TIMES "##close", { 30.f, 28.f })) m_open = false;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

void ModMenu::renderTabBar(ImDrawList* dl, ImVec2 pos, ImVec2 sz) {
    float barY    = pos.y + kHeaderH;
    float padX    = 14.f, padY = 7.f;
    float cntX0   = pos.x + padX;
    float cntY0   = barY + padY;
    float cntH    = kTabBarH - padY * 2.f;
    float totalW  = sz.x - padX * 2.f;
    float tabW    = totalW / kTabCount;

    dl->AddRectFilled({ cntX0, cntY0 }, { cntX0 + totalW, cntY0 + cntH },
        kDark1, cntH * 0.5f);
    dl->AddLine({ pos.x, barY + kTabBarH }, { pos.x + sz.x, barY + kTabBarH }, kBorder, 1.f);

    for (int i = 0; i < kTabCount; i++) {
        bool active = (m_activeTab == i);
        float tx = cntX0 + tabW * i;

        if (active)
            dl->AddRectFilled({ tx + 3.f, cntY0 + 2.f }, { tx + tabW - 3.f, cntY0 + cntH - 2.f },
                kBlurple, (cntH - 4.f) * 0.5f);

        float iw = ImGui::CalcTextSize(kTabs[i].icon).x;
        float lw = ImGui::CalcTextSize(kTabs[i].label).x;
        float cx = tx + (tabW - iw - 4.f - lw) * 0.5f;
        float cy = cntY0 + (cntH - ImGui::GetFontSize()) * 0.5f;
        ImU32 tc = active ? kWhite : kGreyDim;

        dl->AddText({ cx, cy },           tc, kTabs[i].icon);
        dl->AddText({ cx + iw + 4.f, cy }, tc, kTabs[i].label);

        ImGui::SetCursorScreenPos({ tx, cntY0 });
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.04f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.1f  });
        char bid[16]; snprintf(bid, sizeof(bid), "##tab%d", i);
        if (ImGui::Button(bid, { tabW, cntH })) {
            m_activeTab = i; m_selectedModule = nullptr; m_showInfo = false;
        }
        ImGui::PopStyleColor(3);
    }
}

void ModMenu::renderModulesTab() {
    float contentH = kMenuH - kHeaderH - kTabBarH;
    float contentW = kMenuW - ((m_selectedModule || m_showInfo) ? kSettingsW : 0.f);

    ImGui::BeginGroup();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 12.f, 0.f });
    ImGui::BeginChild("##catbar", { contentW, kCatBarH }, false, ImGuiWindowFlags_NoScrollbar);
    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      cPos = ImGui::GetWindowPos();
    float pillH = 26.f;
    float pillY = cPos.y + (kCatBarH - pillH) * 0.5f;
    float px    = cPos.x + 10.f;
    bool  searching = m_searchBuf[0] != '\0';

    for (int i = 0; i < kCatCount; i++) {
        bool active = (!searching && m_activeCat == i);
        float iw = ImGui::CalcTextSize(kCats[i].icon).x;
        float lw = ImGui::CalcTextSize(kCats[i].label).x;
        float pw = iw + lw + 18.f;
        ImVec2 pMin = { px, pillY }, pMax = { px + pw, pillY + pillH };

        if (active)
            dl->AddRectFilled(pMin, pMax, IM_COL32(114,137,218,200), pillH * 0.5f);
        else {
            dl->AddRectFilled(pMin, pMax, kDark1, pillH * 0.5f);
            dl->AddRect(pMin, pMax, IM_COL32(60,65,72,160), pillH * 0.5f, 0, 1.f);
        }

        float ty = pillY + (pillH - ImGui::GetFontSize()) * 0.5f;
        dl->AddText({ px + 7.f, ty }, active ? kWhite : kGreyDim, kCats[i].icon);
        dl->AddText({ px + 9.f + iw, ty }, active ? kWhite : kGreyDim, kCats[i].label);

        ImGui::SetCursorScreenPos(pMin);
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.06f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.12f });
        char bid[16]; snprintf(bid, sizeof(bid), "##cat%d", i);
        if (ImGui::Button(bid, { pw, pillH })) m_activeCat = i;
        ImGui::PopStyleColor(3);
        px += pw + 5.f;
    }

    float searchW = 154.f;
    ImGui::SetCursorPos({ contentW - searchW - 10.f, (kCatBarH - 22.f) * 0.5f });
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        kv4Dark1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kv4Dark1);
    ImGui::PushStyleColor(ImGuiCol_Text,           kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##search", ICON_FA_SEARCH "  Search...", m_searchBuf, sizeof(m_searchBuf));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    float gridH = contentH - kCatBarH;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { kGridPad, kGridPad });
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,  kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,   kv4White);
    ImGui::BeginChild("##modgrid", { contentW, gridH }, false);
    dl = ImGui::GetWindowDrawList();

    const ModuleCategory activeCat = kCats[m_activeCat].cat;
    auto& mods = ModuleManager::get().getModules();
    float scrollY = ImGui::GetScrollY();
    float cardW   = (contentW - kGridPad * 2.f - kGridGap * (kCols - 1)) / kCols;
    int   col     = 0;
    float rowX = kGridPad, rowY = kGridPad;
    bool  any  = false;

    for (auto& modPtr : mods) {
        ModuleBase* mod = modPtr.get();
        bool matchesCat    = searching || mod->getCategory() == activeCat;
        bool matchesSearch = containsCI(mod->getName(), m_searchBuf) ||
                             containsCI(mod->getDescription(), m_searchBuf);
        if (!matchesCat || !matchesSearch) continue;

        any = true;
        ImVec2 wPos = ImGui::GetWindowPos();
        ImVec2 cMin = { wPos.x + rowX, wPos.y + rowY - scrollY };
        ImVec2 cMax = { cMin.x + cardW, cMin.y + kCardH };

        bool enabled  = mod->isEnabled();
        bool selected = (m_selectedModule == mod);
        bool infoSel  = (m_infoModule == mod && m_showInfo);

        ImU32 cardBg = selected ? IM_COL32(114,137,218,20) : (enabled ? IM_COL32(40,44,48,255) : kDark1);
        ImU32 cardBd = selected ? kBorderOn : (enabled ? IM_COL32(114,137,218,90) : IM_COL32(50,55,62,200));

        dl->AddRectFilled(cMin, cMax, cardBg, 10.f);
        dl->AddRect(cMin, cMax, cardBd, 10.f, 0, enabled ? 1.5f : 1.f);

        if (enabled)
            dl->AddRectFilled(cMin, { cMax.x, cMin.y + 3.f },
                IM_COL32(114,137,218,90), 10.f, ImDrawFlags_RoundCornersTop);

        float btnR  = 9.f;
        ImVec2 infoC = { cMin.x + 14.f, cMin.y + 14.f };
        ImVec2 gearC = { cMax.x - 14.f, cMin.y + 14.f };

        bool infoHov = ImGui::IsMouseHoveringRect({ infoC.x - btnR, infoC.y - btnR },
            { infoC.x + btnR, infoC.y + btnR });
        bool hasSet  = !mod->settings().getDefs().empty();
        bool gearHov = hasSet && ImGui::IsMouseHoveringRect(
            { gearC.x - btnR, gearC.y - btnR }, { gearC.x + btnR, gearC.y + btnR });

        dl->AddCircleFilled(infoC, btnR, (infoHov || infoSel) ? kBlurpleDim : IM_COL32(48,52,58,200));
        ImVec2 iSz = ImGui::CalcTextSize(ICON_FA_INFO);
        dl->AddText({ infoC.x - iSz.x * 0.5f, infoC.y - iSz.y * 0.5f },
            (infoHov || infoSel) ? kBlurple : kGreyDim, ICON_FA_INFO);

        if (hasSet) {
            dl->AddCircleFilled(gearC, btnR, (gearHov || selected) ? kBlurpleDim : IM_COL32(48,52,58,200));
            ImVec2 gSz = ImGui::CalcTextSize(ICON_FA_COG);
            dl->AddText({ gearC.x - gSz.x * 0.5f, gearC.y - gSz.y * 0.5f },
                (gearHov || selected) ? kBlurple : kGreyDim, ICON_FA_COG);
        }

        if (!mod->getIcon().empty()) {
            float fs  = 34.f;
            ImVec2 iSzL = ImGui::CalcTextSize(mod->getIcon().c_str());
            float scale = fs / ImGui::GetFontSize();
            float iconW = iSzL.x * scale, iconH2 = iSzL.y * scale;
            float iconAreaTop = cMin.y + 26.f;
            float iconAreaH   = kCardH - 26.f - 30.f;
            dl->AddText(ImGui::GetFont(), fs,
                { cMin.x + (cardW - iconW) * 0.5f,
                  iconAreaTop + (iconAreaH - iconH2) * 0.5f },
                enabled ? kBlurple : IM_COL32(153,170,181,140),
                mod->getIcon().c_str());
        }

        float nameY = cMax.y - 24.f;
        ImVec2 nSz  = ImGui::CalcTextSize(mod->getName().c_str());
        if (enabled) {
            float dotX = cMin.x + (cardW - nSz.x) * 0.5f - 8.f;
            dl->AddCircleFilled({ dotX, nameY + nSz.y * 0.5f }, 3.f, kGreenOn);
        }
        dl->AddText({ cMin.x + (cardW - nSz.x) * 0.5f, nameY },
            enabled ? kWhite : kGrey, mod->getName().c_str());

        ImGui::SetCursorScreenPos(cMin);
        char btnId[96]; snprintf(btnId, sizeof(btnId), "##gc_%s", mod->getName().c_str());
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.03f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.07f });
        if (ImGui::Button(btnId, { cardW, kCardH })) {
            ImVec2 mp = ImGui::GetIO().MousePos;
            bool onInfo2 = ImGui::IsMouseHoveringRect(
                { infoC.x - btnR, infoC.y - btnR }, { infoC.x + btnR, infoC.y + btnR });
            bool onGear2 = hasSet && ImGui::IsMouseHoveringRect(
                { gearC.x - btnR, gearC.y - btnR }, { gearC.x + btnR, gearC.y + btnR });
            if (onInfo2) {
                m_showInfo = !m_showInfo || (m_infoModule != mod);
                m_infoModule = mod; m_selectedModule = nullptr;
            } else if (onGear2) {
                m_selectedModule = selected ? nullptr : mod;
                m_showInfo = false;
            } else {
                mod->toggle();
            }
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDelta.x == 0.f) {
            ImGui::BeginTooltip();
            ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
            ImGui::TextWrapped("%s", mod->getDescription().c_str());
            ImGui::PopStyleColor();
            ImGui::EndTooltip();
        }

        col++;
        if (col >= kCols) { col = 0; rowX = kGridPad; rowY += kCardH + kGridGap; }
        else rowX += cardW + kGridGap;
    }

    if (col != 0) rowY += kCardH + kGridGap;
    ImGui::SetCursorPos({ 0.f, rowY });

    if (!any) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        float tw = ImGui::CalcTextSize("No modules found.").x;
        ImGui::SetCursorPos({ (contentW - tw) * 0.5f, 40.f });
        ImGui::TextUnformatted("No modules found.");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    if (m_selectedModule) { ImGui::SameLine(0.f, 0.f); renderSettingsPanel(m_selectedModule); }
    if (m_showInfo && m_infoModule) { ImGui::SameLine(0.f, 0.f); renderInfoPanel(m_infoModule); }
    ImGui::EndGroup();
}

void ModMenu::renderSettingsPanel(ModuleBase* mod) {
    float panelH = kMenuH - kHeaderH - kTabBarH;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 14.f, 14.f });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kv4Dark1);
    ImGui::BeginChild("##settingspanel", { kSettingsW, panelH }, false);
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();
    dl->AddRectFilled(pos, { pos.x + 1.5f, pos.y + panelH }, kBorder);

    if (!mod->getIcon().empty())
        dl->AddText(nullptr, 16.f, { pos.x + 14.f, pos.y + 14.f }, kBlurple, mod->getIcon().c_str());
    float nameX = mod->getIcon().empty() ? 14.f : 33.f;
    dl->AddText(nullptr, 10.f, { pos.x + nameX, pos.y + 14.f }, kGreyDim, "SETTINGS");
    dl->AddText(nullptr, 13.f, { pos.x + nameX, pos.y + 26.f }, kWhite, mod->getName().c_str());
    dl->AddLine({ pos.x + 8.f, pos.y + 46.f }, { pos.x + kSettingsW - 8.f, pos.y + 46.f }, kBorder);

    ImGui::SetCursorPos({ kSettingsW - 28.f, 8.f });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.2f,0.2f,0.5f });
    ImGui::PushStyleColor(ImGuiCol_Text,          kv4Grey);
    if (ImGui::Button(ICON_FA_TIMES "##cls", { 20.f, 20.f })) m_selectedModule = nullptr;
    ImGui::PopStyleColor(3);
    ImGui::SetCursorPosY(52.f);

    auto& defs = mod->settings().getDefs();
    if (defs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::TextWrapped("No configurable settings.");
        ImGui::PopStyleColor();
    } else {
        for (auto& [key, def] : defs) renderSettingWidget(key, const_cast<SettingDef&>(def));
    }

    ImVec2 kbPos = ImGui::GetCursorScreenPos();
    dl->AddLine({ kbPos.x, kbPos.y + 4.f }, { kbPos.x + kSettingsW - 28.f, kbPos.y + 4.f }, kBorder);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f);
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::Text("Keybind");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 8.f);
    ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
    ImGui::TextUnformatted(vkName(mod->getKey()));
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void ModMenu::renderInfoPanel(ModuleBase* mod) {
    float panelH = kMenuH - kHeaderH - kTabBarH;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 14.f, 14.f });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kv4Dark1);
    ImGui::BeginChild("##infopanel", { kSettingsW, panelH }, false);
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();
    dl->AddRectFilled(pos, { pos.x + 1.5f, pos.y + panelH }, kBorder);

    if (!mod->getIcon().empty())
        dl->AddText(nullptr, 18.f, { pos.x + 14.f, pos.y + 12.f }, kBlurple, mod->getIcon().c_str());
    float nameX = mod->getIcon().empty() ? 14.f : 36.f;
    dl->AddText(nullptr, 10.f, { pos.x + nameX, pos.y + 14.f }, kGreyDim, "MODULE INFO");
    dl->AddText(nullptr, 13.f, { pos.x + nameX, pos.y + 26.f }, kWhite, mod->getName().c_str());
    dl->AddLine({ pos.x + 8.f, pos.y + 48.f }, { pos.x + kSettingsW - 8.f, pos.y + 48.f }, kBorder);

    ImGui::SetCursorPos({ kSettingsW - 28.f, 8.f });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.2f,0.2f,0.5f });
    ImGui::PushStyleColor(ImGuiCol_Text,          kv4Grey);
    if (ImGui::Button(ICON_FA_TIMES "##cli", { 20.f, 20.f })) m_showInfo = false;
    ImGui::PopStyleColor(3);
    ImGui::SetCursorPosY(54.f);

    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextWrapped("%s", mod->getDescription().c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing(); ImGui::Spacing();

    auto infoRow = [&](const char* lbl, const char* val) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey); ImGui::Text("%-10s", lbl); ImGui::PopStyleColor();
        ImGui::SameLine(0.f, 4.f);
        ImGui::PushStyleColor(ImGuiCol_Text, kv4White); ImGui::TextUnformatted(val); ImGui::PopStyleColor();
    };
    infoRow("Category:", categoryName(mod->getCategory()));
    infoRow("Status:",   mod->isEnabled() ? "Enabled" : "Disabled");
    infoRow("Keybind:",  vkName(mod->getKey()));

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void ModMenu::renderElementsTab() {
    float panelH = kMenuH - kHeaderH - kTabBarH;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 18.f, 18.f });
    ImGui::BeginChild("##elements", { kMenuW, panelH }, false);
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();
    dl->AddText(nullptr, 11.f, { pos.x + 18.f, pos.y + 18.f }, kBlurple, "HUD ELEMENTS");
    dl->AddLine({ pos.x + 18.f, pos.y + 32.f }, { pos.x + kMenuW - 18.f, pos.y + 32.f }, kBorder);
    ImGui::SetCursorPosY(40.f);
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextWrapped("Drag HUD modules to reposition them. Enable them below.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    for (auto& mp : ModuleManager::get().getModules()) {
        if (mp->getCategory() != ModuleCategory::HUD) continue;
        ImVec2 cMin = ImGui::GetCursorScreenPos();
        float cW = kMenuW - 36.f, cH = 46.f;
        ImVec2 cMax = { cMin.x + cW, cMin.y + cH };
        dl->AddRectFilled(cMin, cMax, mp->isEnabled() ? IM_COL32(114,137,218,18) : kDark1, 8.f);
        dl->AddRect(cMin, cMax, mp->isEnabled() ? kBorderOn : IM_COL32(50,55,62,180), 8.f, 0, 1.f);
        if (!mp->getIcon().empty())
            dl->AddText(nullptr, 14.f, { cMin.x + 12.f, cMin.y + (cH - 14.f) * 0.5f },
                mp->isEnabled() ? kBlurple : kGreyDim, mp->getIcon().c_str());
        dl->AddText(nullptr, 13.f, { cMin.x + 32.f, cMin.y + 9.f }, kWhite, mp->getName().c_str());
        char pb[48]; snprintf(pb, sizeof(pb), "%.0f, %.0f", mp->getPos().x, mp->getPos().y);
        ImVec2 pbSz = ImGui::CalcTextSize(pb);
        dl->AddText(nullptr, 11.f, { cMax.x - pbSz.x - 12.f, cMin.y + 9.f }, kGreyDim, pb);
        ImGui::SetCursorScreenPos(cMin);
        char bid[96]; snprintf(bid, sizeof(bid), "##el_%s", mp->getName().c_str());
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.04f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.08f });
        if (ImGui::Button(bid, { cW, cH })) mp->toggle();
        ImGui::PopStyleColor(3);
        ImGui::SetCursorScreenPos({ cMin.x, cMax.y + 6.f });
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void ModMenu::renderEditorsTab() {
    float panelH = kMenuH - kHeaderH - kTabBarH;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 18.f, 18.f });
    ImGui::BeginChild("##editors", { kMenuW, panelH }, false);
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();
    dl->AddText(nullptr, 11.f, { pos.x + 18.f, pos.y + 18.f }, kBlurple, "KEYBIND EDITOR");
    dl->AddLine({ pos.x + 18.f, pos.y + 32.f }, { pos.x + kMenuW - 18.f, pos.y + 32.f }, kBorder);
    ImGui::SetCursorPosY(40.f);
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextWrapped("Assign hotkeys to modules. Only bound modules are shown.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    for (auto& mp : ModuleManager::get().getModules()) {
        if (mp->getKey() == 0) continue;
        ImVec2 rMin = ImGui::GetCursorScreenPos();
        float rW = kMenuW - 36.f, rH = 36.f;
        ImVec2 rMax = { rMin.x + rW, rMin.y + rH };
        dl->AddRectFilled(rMin, rMax, kDark1, 6.f);
        dl->AddRect(rMin, rMax, IM_COL32(50,55,62,160), 6.f, 0, 1.f);
        dl->AddText(nullptr, 12.f, { rMin.x + 12.f, rMin.y + (rH - 12.f) * 0.5f },
            kWhite, mp->getName().c_str());
        const char* kb = vkName(mp->getKey());
        ImVec2 kbSz = ImGui::CalcTextSize(kb);
        float kbX = rMax.x - kbSz.x - 20.f;
        dl->AddRectFilled({ kbX - 6.f, rMin.y + 6.f }, { kbX + kbSz.x + 6.f, rMax.y - 6.f },
            IM_COL32(114,137,218,28), 4.f);
        dl->AddRect({ kbX - 6.f, rMin.y + 6.f }, { kbX + kbSz.x + 6.f, rMax.y - 6.f },
            IM_COL32(114,137,218,80), 4.f, 0, 1.f);
        dl->AddText(nullptr, 11.f, { kbX, rMin.y + (rH - 11.f) * 0.5f }, kBlurple, kb);
        ImGui::SetCursorScreenPos({ rMin.x, rMax.y + 4.f });
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void ModMenu::renderInformationTab() {
    float panelH = kMenuH - kHeaderH - kTabBarH;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 18.f, 18.f });
    ImGui::BeginChild("##information", { kMenuW, panelH }, false);
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();

    auto section = [&](const char* title) {
        ImVec2 sp = ImGui::GetCursorScreenPos();
        dl->AddText(nullptr, 11.f, sp, kBlurple, title);
        dl->AddLine({ sp.x, sp.y + 14.f }, { sp.x + kMenuW - 36.f, sp.y + 14.f }, kBorder);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.f);
    };
    auto infoRow = [&](const char* lbl, const char* val) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey); ImGui::Text("%-20s", lbl); ImGui::PopStyleColor();
        ImGui::SameLine(0.f, 4.f);
        ImGui::PushStyleColor(ImGuiCol_Text, kv4White); ImGui::TextUnformatted(val); ImGui::PopStyleColor();
    };

    section("CLIENT INFO");
    infoRow("Name:",         "Glacier Client");
    infoRow("Version:",      GLACIER_VERSION);
    infoRow("Target:",       "Minecraft Bedrock v26.x");
    infoRow("Renderer:",     "DirectX 11 + ImGui");
    infoRow("Architecture:", "x64 DLL Injection");

    ImGui::Spacing(); ImGui::Spacing();
    section("MENU KEYBIND");
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextWrapped("The key used to open and close this menu.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImVec2 kboxMin = ImGui::GetCursorScreenPos();
    ImVec2 kboxMax = { kboxMin.x + kMenuW - 36.f, kboxMin.y + 34.f };
    dl->AddRectFilled(kboxMin, kboxMax, m_capturingKey ? IM_COL32(114,137,218,40) : kDark1, 6.f);
    dl->AddRect(kboxMin, kboxMax, m_capturingKey ? kBlurple : IM_COL32(60,65,72,255), 6.f, 0,
        m_capturingKey ? 1.5f : 1.f);
    const char* klbl = m_capturingKey ? ICON_FA_KEYBOARD "  Press any key..." : vkName(ClientConfig::get().menuKey);
    ImVec2 kls = ImGui::CalcTextSize(klbl);
    dl->AddText({ kboxMin.x + ((kboxMax.x - kboxMin.x) - kls.x) * 0.5f,
                  kboxMin.y + ((kboxMax.y - kboxMin.y) - kls.y) * 0.5f },
        m_capturingKey ? kBlurple : kWhite, klbl);
    ImGui::SetCursorScreenPos(kboxMin);
    ImGui::PushStyleColor(ImGuiCol_Button, { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.04f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.08f });
    if (ImGui::Button("##kbind", { kMenuW - 36.f, 34.f })) m_capturingKey = !m_capturingKey;
    ImGui::PopStyleColor(3);
    ImGui::SetCursorScreenPos({ kboxMin.x, kboxMax.y + 8.f });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextWrapped("Click the box, then press the desired key.");
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Spacing();
    section("STATISTICS");
    int total = 0, enabled = 0;
    for (auto& m : ModuleManager::get().getModules()) { total++; if (m->isEnabled()) enabled++; }
    char sb[32];
    snprintf(sb, sizeof(sb), "%d", total);   infoRow("Total Modules:", sb);
    snprintf(sb, sizeof(sb), "%d", enabled); infoRow("Active:",        sb);

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void ModMenu::renderSettingWidget(const std::string& key, SettingDef& def) {
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextUnformatted(def.label.c_str());
    ImGui::PopStyleColor();
    float w = kSettingsW - 28.f;
    ImGui::SetNextItemWidth(w);
    char wid[128]; snprintf(wid, sizeof(wid), "##s_%s", key.c_str());
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          kv4Dark3);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   kv4Dark3);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,        kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,  kv4White);
    ImGui::PushStyleColor(ImGuiCol_CheckMark,         kv4Blurple);
    if (std::holds_alternative<bool>(def.value)) {
        bool v = std::get<bool>(def.value); if (ImGui::Checkbox(wid, &v)) def.value = v;
    } else if (std::holds_alternative<int>(def.value)) {
        int v = std::get<int>(def.value), mn = std::get<int>(def.minVal), mx = std::get<int>(def.maxVal);
        if (ImGui::SliderInt(wid, &v, mn, mx)) def.value = v;
    } else if (std::holds_alternative<float>(def.value)) {
        float v = std::get<float>(def.value), mn = std::get<float>(def.minVal), mx = std::get<float>(def.maxVal);
        if (ImGui::SliderFloat(wid, &v, mn, mx, "%.1f")) def.value = v;
    } else if (std::holds_alternative<std::string>(def.value)) {
        auto& sv = std::get<std::string>(def.value);
        char buf[256]{}; strncpy_s(buf, sv.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText(wid, buf, sizeof(buf))) sv = buf;
    }
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar();
    ImGui::Spacing();
}
