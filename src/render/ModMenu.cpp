#include "ModMenu.h"
#include "../modules/ModuleManager.h"
#include "../utils/ClientConfig.h"
#include "../icons/IconManager.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <cstdio>
#include <cmath>

// ── Palette ────────────────────────────────────────────────────────────────────
static constexpr ImU32 kBlurple   = IM_COL32(114, 137, 218, 255);
static constexpr ImU32 kBlurpleDm = IM_COL32(114, 137, 218,  70);
static constexpr ImU32 kBg0       = IM_COL32( 10,  10,  12, 255); // deepest
static constexpr ImU32 kBg1       = IM_COL32( 16,  16,  20, 255); // base
static constexpr ImU32 kBg2       = IM_COL32( 22,  22,  28, 255); // elements/cards
static constexpr ImU32 kBg3       = IM_COL32( 26,  26,  34, 255); // hovered elements
static constexpr ImU32 kBorder    = IM_COL32(255, 255, 255,  15);
static constexpr ImU32 kBorderOn  = IM_COL32(114, 137, 218, 160);
static constexpr ImU32 kWhite     = IM_COL32(255, 255, 255, 255);
static constexpr ImU32 kGrey      = IM_COL32(155, 160, 175, 220);
static constexpr ImU32 kGreyDim   = IM_COL32(100, 105, 120, 180);
static constexpr ImU32 kGreen     = IM_COL32( 67, 181, 129, 255);
static constexpr ImU32 kRed       = IM_COL32(220,  60,  60, 255);
static constexpr ImU32 kShadow    = IM_COL32(  0,   0,   0, 200);

static constexpr ImVec4 kv4Blurple { 0.447f, 0.537f, 0.855f, 1.f };
static constexpr ImVec4 kv4Grey    { 0.608f, 0.627f, 0.686f, 0.86f };
static constexpr ImVec4 kv4White   { 1.f,    1.f,    1.f,    1.f };

// ── Category descriptors ───────────────────────────────────────────────────────
struct CatDesc { const char* label; ModuleCategory cat; };
static constexpr CatDesc kCats[] = {
    { "HUD",      ModuleCategory::HUD      },
    { "Combat",   ModuleCategory::Combat   },
    { "Movement", ModuleCategory::Movement },
    { "Visual",   ModuleCategory::Visual   },
    { "Utility",  ModuleCategory::Utility  },
};
static constexpr int kCatCount = 5;

// ── Helpers ────────────────────────────────────────────────────────────────────
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
    if (vk >= 'A' && vk <= 'Z') { buf[0]=(char)vk; buf[1]=0; return buf; }
    if (vk >= '0' && vk <= '9') { buf[0]=(char)vk; buf[1]=0; return buf; }
    if (vk >= VK_F1 && vk <= VK_F12) { snprintf(buf,sizeof(buf),"F%d",vk-VK_F1+1); return buf; }
    switch(vk){ case VK_INSERT:return"INS"; case VK_DELETE:return"DEL";
                case VK_HOME:return"HOME"; case VK_END:return"END";
                case VK_SPACE:return"SPC"; case VK_RETURN:return"ENT"; }
    snprintf(buf,sizeof(buf),"0x%02X",vk); return buf;
}

static void shadow(ImDrawList* dl, float sz, ImVec2 p, const char* t, ImU32 col) {
    dl->AddText(nullptr, sz, { p.x+1.5f, p.y+1.5f }, kShadow, t);
    dl->AddText(nullptr, sz, p, col, t);
}

// ── Singleton ──────────────────────────────────────────────────────────────────
ModMenu& ModMenu::get() { static ModMenu i; return i; }
void ModMenu::init() {}

// ─────────────────────────────────────────────────────────────────────────────
//  render()
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::render() {
    if (!m_open) return;

    if (m_capturingKey) {
        for (int vk = 8; vk <= 0xFE; ++vk) {
            if (vk==VK_LBUTTON||vk==VK_RBUTTON||vk==VK_MBUTTON) continue;
            if (GetAsyncKeyState(vk) & 0x8001) {
                ClientConfig::get().menuKey = vk;
                m_capturingKey = false;
                break;
            }
        }
    }

    m_transAlpha = m_transAlpha + (1.f - m_transAlpha) * 0.18f;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ (io.DisplaySize.x - W)*0.5f, (io.DisplaySize.y - H)*0.5f }, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ W, H }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    { 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   ROUND);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,  0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("##gm", &m_open, wf)) {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetWindowPos();

        for (int i = 6; i >= 1; --i)
            dl->AddRectFilled({ pos.x+(float)i, pos.y+(float)i },
                              { pos.x+W+(float)i, pos.y+H+(float)i },
                              IM_COL32(0,0,0,15), ROUND+4.f);

        dl->AddRectFilled(pos, { pos.x+W, pos.y+H }, kBg0, ROUND);
        dl->AddRect(pos, { pos.x+W, pos.y+H }, kBorder, ROUND, 0, 1.2f);

        renderHeaderAndTabs(W);

        float topH = HDR_H + CAT_H;
        ImGui::SetCursorPos({ 0.f, topH });
        
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_transAlpha);
        switch (m_screen) {
            case Screen::Grid:     renderGridSubPage();     break;
            case Screen::Settings: renderSettingsSubPage(); break;
        }
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Header And Tabs (Top Bar)
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderHeaderAndTabs(float panW) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();

    // 1. Header Row
    dl->AddRectFilled(pos, { pos.x+panW, pos.y+HDR_H }, kBg2, ROUND, ImDrawFlags_RoundCornersTop);
    
    // Logo
    ImGui::SetCursorPos({ 20.f, 20.f });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Blurple);
    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text("GLACIER");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine(0.f, 8.f);
    ImGui::SetCursorPosY(24.f);
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextUnformatted("v" GLACIER_VERSION);
    ImGui::PopStyleColor();

    // Close Button
    ImGui::SetCursorPos({ panW - 46.f, 15.f });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.18f,0.18f,0.75f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.8f,0.18f,0.18f,1.f  });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::Button("X##close", { 30.f, 30.f })) m_open = false;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // 2. Tabs Row
    float barY = pos.y + HDR_H;
    dl->AddRectFilled({ pos.x, barY }, { pos.x+panW, barY+CAT_H }, kBg1);
    dl->AddLine({ pos.x, barY+CAT_H }, { pos.x+panW, barY+CAT_H }, kBorder, 1.f);

    float px = pos.x + 16.f;
    float py = barY + (CAT_H - 30.f) * 0.5f;

    for (int i = 0; i < kCatCount; i++) {
        bool active = (m_activeCat == i && m_screen == Screen::Grid);
        
        float lw = ImGui::CalcTextSize(kCats[i].label).x;
        float pw = lw + 24.f;
        ImVec2 pMin = { px, py }, pMax = { px+pw, py+30.f };

        if (active) {
            dl->AddRectFilled(pMin, pMax, kBlurple, 15.f);
        } else {
            bool hovered = ImGui::IsMouseHoveringRect(pMin, pMax);
            dl->AddRectFilled(pMin, pMax, hovered ? kBg3 : IM_COL32(0,0,0,0), 15.f);
        }

        ImVec2 ts = ImGui::CalcTextSize(kCats[i].label);
        dl->AddText(nullptr, 14.f,
            { px + (pw - ts.x)*0.5f, py + (30.f - ts.y)*0.5f },
            active ? kWhite : kGrey, kCats[i].label);

        ImGui::SetCursorScreenPos(pMin);
        ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0,0,0,0});
        char bid[16]; snprintf(bid,sizeof(bid),"##cat%d",i);
        if (ImGui::Button(bid, { pw, 30.f })) {
            m_activeCat = i;
            if (m_screen != Screen::Grid) {
                m_screen = Screen::Grid;
                m_transAlpha = 0.f;
            }
        }
        ImGui::PopStyleColor(3);
        px += pw + 8.f;
    }

    // Search Box
    float searchW = 180.f;
    float sx = pos.x + panW - searchW - 16.f;
    ImGui::SetCursorScreenPos({ sx, barY + (CAT_H - 28.f)*0.5f });
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.06f,0.06f,0.08f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.09f,0.09f,0.11f,1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,            kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    ImGui::SetNextItemWidth(searchW);
    // If we type in search, automatically go to grid view
    if (ImGui::InputTextWithHint("##srch", "Search...", m_searchBuf, sizeof(m_searchBuf))) {
        if (m_screen != Screen::Grid) {
            m_screen = Screen::Grid;
            m_transAlpha = 0.f;
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Toggle Switch Widget
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderToggleSwitch(float x, float y, bool active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float width = 36.f;
    float height = 20.f;
    float radius = height * 0.5f;

    ImU32 bgCol = active ? kBlurple : IM_COL32(50, 52, 60, 255);
    dl->AddRectFilled({x, y}, {x + width, y + height}, bgCol, radius);

    float knobRadius = radius - 2.f;
    float knobX = active ? (x + width - knobRadius - 2.f) : (x + knobRadius + 2.f);
    dl->AddCircleFilled({knobX, y + radius}, knobRadius, kWhite);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Module card
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderModuleCard(ModuleBase* mod, float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool  en = mod->isEnabled();
    
    // Hover logic
    ImRect bb({x, y}, {x+w, y+h});
    bool hov = ImGui::IsMouseHoveringRect(bb.Min, bb.Max);

    // Card background
    ImU32 cardBg = en ? IM_COL32(22,25,35,255) : (hov ? kBg3 : kBg2);
    dl->AddRectFilled(bb.Min, bb.Max, cardBg, 12.f);

    if (en) {
        dl->AddRectFilledMultiColor(
            bb.Min, { bb.Min.x+w, bb.Min.y+4.f },
            kBlurple, kBlurple,
            IM_COL32(114,137,218,0), IM_COL32(114,137,218,0));
    }
    dl->AddRect(bb.Min, bb.Max, en ? IM_COL32(114,137,218,120) : kBorder, 12.f, 0, 1.2f);

    // Layout
    float iconSize = 40.f;
    float pad = 14.f;

    // Header row: Icon & Toggle
    auto* srv = IconManager::get().getIcon(mod->getIconName());
    if (srv) {
        ImU32 tint = en ? IM_COL32(255,255,255,255) : IM_COL32(140,145,160,200);
        dl->AddImage((ImTextureID)srv,
            { x+pad, y+pad }, { x+pad+iconSize, y+pad+iconSize },
            { 0,0 }, { 1,1 }, tint);
    } else {
        ImU32 tileCol = en ? kBlurpleDm : IM_COL32(40,42,50,200);
        dl->AddRectFilled({ x+pad, y+pad }, { x+pad+iconSize, y+pad+iconSize }, tileCol, 6.f);
    }

    // Toggle switch hitbox
    float togW = 36.f, togH = 20.f;
    float togX = x + w - pad - togW;
    float togY = y + pad;
    ImRect togBb({togX, togY}, {togX+togW, togY+togH});
    bool togHov = ImGui::IsMouseHoveringRect(togBb.Min, togBb.Max);

    renderToggleSwitch(togX, togY, en);

    // Toggling interaction
    ImGui::SetCursorScreenPos(togBb.Min);
    char tid[64]; snprintf(tid,sizeof(tid),"##tog_%s",mod->getName().c_str());
    ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0,0,0,0.1f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0,0,0,0.2f});
    if (ImGui::InvisibleButton(tid, {togW, togH})) {
        mod->toggle();
    }
    if (ImGui::IsItemClicked(0)) { /* Handled by InvisibleButton return */ }
    ImGui::PopStyleColor(3);

    // Module Name & Description
    float textY = y + pad + iconSize + 10.f;
    dl->AddText(nullptr, 16.f, { x+pad, textY }, en ? kWhite : kGrey, mod->getName().c_str());
    
    // Truncate desc if too long
    std::string desc = mod->getDescription();
    if (desc.size() > 45) desc = desc.substr(0, 42) + "...";
    dl->AddText(nullptr, 13.f, { x+pad, textY + 22.f }, kGreyDim, desc.c_str());

    // Card Interaction (Open settings)
    ImGui::SetCursorScreenPos(bb.Min);
    char cid[64]; snprintf(cid,sizeof(cid),"##card_%s",mod->getName().c_str());
    // We cover the whole card, but since InvisibleButton for toggle was rendered earlier, 
    // it will steal focus if hovered. Wait, Z-order: ImGui naturally resolves overlapping buttons 
    // by evaluating last-rendered on top, so we should render Card Button FIRST, THEN toggle.
    // However, it's safer to just do a mathematical check.
    ImGui::InvisibleButton(cid, {w, h});
    bool cardClicked = ImGui::IsItemClicked(0) || ImGui::IsItemClicked(1);
    if (cardClicked && !togHov) { // Left or right click opens settings unless on toggle
        m_selectedMod = mod;
        m_screen = Screen::Settings;
        m_transAlpha = 0.f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  GRID Sub-Page
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderGridSubPage() {
    float contentY = HDR_H + CAT_H;
    float contentH = H - contentY;
    float cardW    = (W - GRID_PAD*2 - CARD_GAP*(COLS-1)) / COLS;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { GRID_PAD, GRID_PAD });
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,  kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,   kv4White);
    
    ImGui::BeginChild("##grid", { W, contentH }, false);

    auto& mods = ModuleManager::get().getModules();
    const ModuleCategory cat = kCats[m_activeCat].cat;
    bool searching = m_searchBuf[0] != '\0';
    float scrollY  = ImGui::GetScrollY();
    ImVec2 wPos    = ImGui::GetWindowPos();

    int  col = 0;
    float rx = GRID_PAD, ry = GRID_PAD;
    bool any = false;

    for (auto& mp : mods) {
        ModuleBase* mod = mp.get();
        bool matchCat  = mod->getCategory() == cat;
        bool matchSrch = containsCI(mod->getName(), m_searchBuf) ||
                         containsCI(mod->getDescription(), m_searchBuf);
        
        if (searching && !matchSrch) continue;
        if (!searching && !matchCat) continue;
        
        any = true;
        float cx = wPos.x + rx;
        float cy = wPos.y + ry - scrollY;
        renderModuleCard(mod, cx, cy, cardW, CARD_H);

        col++;
        if (col >= COLS) { col=0; rx=GRID_PAD; ry+=CARD_H+CARD_GAP; }
        else rx += cardW + CARD_GAP;
    }
    if (col != 0) ry += CARD_H + CARD_GAP;
    ImGui::SetCursorPos({ 0.f, ry }); // Expand child bounds

    if (!any) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        float tw = ImGui::CalcTextSize("No modules found...").x;
        ImGui::SetCursorPos({ (W - tw)*0.5f, 60.f });
        ImGui::TextUnformatted("No modules found...");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SETTINGS Sub-Page
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderSettingsSubPage() {
    if (!m_selectedMod) { m_screen = Screen::Grid; return; }

    float contentY = HDR_H + CAT_H;
    float contentH = H - contentY;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wPos = ImGui::GetWindowPos();
    ModuleBase* mod = m_selectedMod;

    // Background transition
    dl->AddRectFilled({wPos.x, wPos.y + contentY}, {wPos.x + W, wPos.y + H}, kBg0, ROUND, ImDrawFlags_RoundCornersBottom);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 24.f, 20.f });
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,   kv4White);
    
    ImGui::BeginChild("##settings_body", { W, contentH }, false);

    // --- Profile Header ---
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f,0.12f,0.16f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kv4White);
    ImGui::PushStyleColor(ImGuiCol_Text,          kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::Button("< Back", { 80.f, 32.f })) {
        m_screen     = Screen::Grid;
        m_transAlpha = 0.f;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.f, 20.f);

    ImVec2 cp = ImGui::GetCursorPos();
    auto* srv = IconManager::get().getIcon(mod->getIconName());
    if (srv) {
        ImGui::Image((ImTextureID)srv, {40.f, 40.f});
        ImGui::SameLine(0.f, 15.f);
    }
    
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("%s", mod->getName().c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    
    ImGui::PushStyleColor(ImGuiCol_Text, kv4GreyDim);
    ImGui::Text("%s", mod->getDescription().c_str());
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    // Toggle button right aligned
    ImGui::SameLine(W - 24.f - 60.f - cp.x); // Alignment padding
    ImGui::SetCursorPosY(cp.y + 4.f);
    
    bool en = mod->isEnabled();
    ImVec2 togP = ImGui::GetCursorScreenPos();
    renderToggleSwitch(togP.x + 20.f, togP.y, en);
    
    ImGui::SetCursorScreenPos({togP.x + 20.f, togP.y});
    if (ImGui::InvisibleButton("##mastTog", {40.f, 20.f})) {
        mod->toggle();
    }

    ImGui::Spacing(); ImGui::Spacing();
    
    // Line separator
    ImVec2 lp = ImGui::GetCursorScreenPos();
    dl->AddLine(lp, { lp.x + W - 48.f, lp.y }, kBorder, 1.f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15.f);

    // --- Settings List ---
    auto& defs = mod->settings().getDefs();
    if (defs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::TextWrapped("This module has no configurable settings.");
        ImGui::PopStyleColor();
    } else {
        for (auto& [key, def] : defs) {
            renderSettingRow(key, const_cast<SettingDef&>(def), W - 48.f);
        }
    }

    // Keybind
    ImGui::Spacing();
    ImVec2 kbp = ImGui::GetCursorScreenPos();
    dl->AddLine(kbp, { kbp.x + W - 48.f, kbp.y }, kBorder, 1.f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15.f);

    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::Text("Keybind");
    ImGui::PopStyleColor();
    ImGui::SameLine(W - 48.f - 60.f);
    
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f,0.12f,0.16f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f,0.15f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kv4Blurple);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    char kbtn[32]; snprintf(kbtn, sizeof(kbtn), "%s##kb", vkName(mod->getKey()));
    if (ImGui::Button(kbtn, { 60.f, 24.f })) {
        // Simple mock of binding logic for UI, actual logic might differ
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::EndChild();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Setting widget row
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderSettingRow(const std::string& key, SettingDef& def, float rowW) {
    ImGui::PushID(key.c_str());

    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextUnformatted(def.label.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(rowW * 0.45f);
    ImGui::SetNextItemWidth(rowW * 0.55f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.06f,0.06f,0.08f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   ImVec4(0.09f,0.09f,0.12f,1.f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,        kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,  kv4White);
    ImGui::PushStyleColor(ImGuiCol_CheckMark,         kv4Blurple);

    char wid[64]; snprintf(wid,sizeof(wid),"##sw_%s",key.c_str());

    if (std::holds_alternative<bool>(def.value)) {
        bool v = std::get<bool>(def.value);
        if (ImGui::Checkbox(wid, &v)) def.value = v;
    } else if (std::holds_alternative<int>(def.value)) {
        int v=std::get<int>(def.value), mn=std::get<int>(def.minVal), mx=std::get<int>(def.maxVal);
        if (ImGui::SliderInt(wid, &v, mn, mx)) def.value = v;
    } else if (std::holds_alternative<float>(def.value)) {
        float v=std::get<float>(def.value), mn=std::get<float>(def.minVal), mx=std::get<float>(def.maxVal);
        if (ImGui::SliderFloat(wid, &v, mn, mx, "%.2f")) def.value = v;
    } else if (std::holds_alternative<std::string>(def.value)) {
        auto& sv = std::get<std::string>(def.value);
        char buf[256]{}; strncpy_s(buf, sv.c_str(), sizeof(buf)-1);
        if (ImGui::InputText(wid, buf, sizeof(buf))) sv = buf;
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar();
    
    ImGui::Spacing();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 lp = ImGui::GetCursorScreenPos();
    dl->AddLine(lp, { lp.x + rowW, lp.y }, kBorder, 0.4f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.f);
    
    ImGui::PopID();
}
