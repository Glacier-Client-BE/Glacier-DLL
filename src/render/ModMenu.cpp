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
static constexpr ImU32 kBg1       = IM_COL32( 18,  18,  22, 255); // card bg
static constexpr ImU32 kBg2       = IM_COL32( 26,  26,  32, 255); // header / cat bar
static constexpr ImU32 kBg3       = IM_COL32( 32,  34,  42, 255); // settings panel
static constexpr ImU32 kBorder    = IM_COL32(114, 137, 218,  38);
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

    // Keybind capture
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

    // Smooth transition alpha
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

        // ── Drop shadows ──────────────────────────────────────────────────
        for (int i = 6; i >= 1; --i)
            dl->AddRectFilled({ pos.x+(float)i, pos.y+(float)i },
                              { pos.x+W+(float)i, pos.y+H+(float)i },
                              IM_COL32(0,0,0,15), ROUND+4.f);

        // ── Window background ─────────────────────────────────────────────
        dl->AddRectFilled(pos, { pos.x+W, pos.y+H }, kBg0, ROUND);
        dl->AddRect(pos, { pos.x+W, pos.y+H }, kBorder, ROUND, 0, 1.2f);

        switch (m_screen) {
            case Screen::Grid:     renderGrid();           break;
            case Screen::Settings: renderSettingsScreen(); break;
            case Screen::Info:     renderInfoScreen();     break;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Header (logo + close)
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderHeader(float panW) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();

    // Header bg — subtle gradient via two rects
    dl->AddRectFilled(pos, { pos.x+panW, pos.y+HDR_H }, kBg2, ROUND, ImDrawFlags_RoundCornersTop);
    dl->AddLine({ pos.x, pos.y+HDR_H }, { pos.x+panW, pos.y+HDR_H }, kBorder, 1.f);

    // Logo
    ImGui::SetCursorPos({ 18.f, 14.f });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Blurple);
    ImGui::Text("GLACIER");
    ImGui::PopStyleColor();

    ImGui::SameLine(0.f, 8.f);
    ImVec2 vp = ImGui::GetCursorScreenPos();
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextUnformatted("v" GLACIER_VERSION);
    ImGui::PopStyleColor();

    // Close button
    ImGui::SetCursorPos({ panW - 42.f, 9.f });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.18f,0.18f,0.75f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.8f,0.18f,0.18f,1.f  });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::Button("X##close", { 30.f, 30.f })) m_open = false;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Category pill bar
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderCategoryBar(float panW) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();
    float barY = pos.y + HDR_H;

    dl->AddRectFilled({ pos.x, barY }, { pos.x+panW, barY+CAT_H }, kBg2);
    dl->AddLine({ pos.x, barY+CAT_H }, { pos.x+panW, barY+CAT_H }, kBorder, 1.f);

    float px = pos.x + 14.f;
    float py = barY + (CAT_H - 24.f) * 0.5f;
    bool  searching = m_searchBuf[0] != '\0';

    for (int i = 0; i < kCatCount; i++) {
        bool active = !searching && m_activeCat == i;
        float lw = ImGui::CalcTextSize(kCats[i].label).x * 12.f / ImGui::GetFontSize();
        float pw = lw + 20.f;
        ImVec2 pMin = { px, py }, pMax = { px+pw, py+24.f };

        if (active) {
            dl->AddRectFilled(pMin, pMax, kBlurple, 12.f);
        } else {
            dl->AddRectFilled(pMin, pMax, kBg1, 12.f);
            dl->AddRect(pMin, pMax, IM_COL32(60,65,80,180), 12.f, 0, 1.f);
        }

        ImVec2 ts = ImGui::CalcTextSize(kCats[i].label);
        float  sc2 = 12.f / ImGui::GetFontSize();
        dl->AddText(nullptr, 12.f,
            { px + (pw - ts.x*sc2)*0.5f, py + (24.f - ts.y*sc2)*0.5f },
            active ? kWhite : kGreyDim, kCats[i].label);

        ImGui::SetCursorScreenPos(pMin);
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.06f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.12f });
        char bid[16]; snprintf(bid,sizeof(bid),"##cat%d",i);
        if (ImGui::Button(bid, { pw, 24.f })) m_activeCat = i;
        ImGui::PopStyleColor(3);
        px += pw + 5.f;
    }

    // Search box on the right
    float searchW = 160.f;
    float sx = pos.x + panW - searchW - 14.f;
    ImGui::SetCursorScreenPos({ sx, barY + (CAT_H - 22.f)*0.5f });
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.07f,0.07f,0.09f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.1f, 0.1f, 0.13f,1.f));
    ImGui::PushStyleColor(ImGuiCol_Text,            kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##srch", "Search modules...", m_searchBuf, sizeof(m_searchBuf));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Module card
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderModuleCard(ModuleBase* mod, float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool  en = mod->isEnabled();

    // Card background
    ImU32 cardBg = en ? IM_COL32(22,24,30,255) : kBg1;
    dl->AddRectFilled({ x, y }, { x+w, y+h }, cardBg, 10.f);

    // Enabled: top glow strip
    if (en) {
        dl->AddRectFilledMultiColor(
            { x, y }, { x+w, y+3.f },
            kBlurple, kBlurple,
            IM_COL32(114,137,218,0), IM_COL32(114,137,218,0));
        // Bottom thin line
        dl->AddRectFilled({ x, y+h-2.f }, { x+w, y+h }, kBlurpleDm, 0.f, ImDrawFlags_RoundCornersBottom);
    }

    dl->AddRect({ x, y }, { x+w, y+h }, en ? kBorderOn : kBorder, 10.f, 0, en ? 1.5f : 1.f);

    // ── Icon (texture) ────────────────────────────────────────────────────
    float iconSize = 48.f;
    float iconX = x + (w - iconSize) * 0.5f;
    float iconY = y + 22.f;

    auto* srv = IconManager::get().getIcon(mod->getIconName());
    if (srv) {
        // Tinted icon box
        ImU32 tint = en ? IM_COL32(255,255,255,240) : IM_COL32(140,145,160,180);
        dl->AddImage((ImTextureID)srv,
            { iconX, iconY }, { iconX+iconSize, iconY+iconSize },
            { 0,0 }, { 1,1 }, tint);
    } else {
        // Fallback colored tile
        ImU32 tileCol = en ? kBlurpleDm : IM_COL32(40,42,50,200);
        dl->AddRectFilled({ iconX, iconY }, { iconX+iconSize, iconY+iconSize }, tileCol, 6.f);
    }

    // ── Module name ───────────────────────────────────────────────────────
    float nameY = y + h - 30.f;
    ImVec2 nSz  = ImGui::CalcTextSize(mod->getName().c_str());
    float  nFs  = 13.f;
    float  nSc  = nFs / ImGui::GetFontSize();
    shadow(dl, nFs, { x + (w - nSz.x*nSc)*0.5f, nameY },
           mod->getName().c_str(), en ? kWhite : kGrey);

    // Enabled dot
    if (en) {
        float dotX = x + (w - nSz.x*nSc)*0.5f - 9.f;
        dl->AddCircleFilled({ dotX, nameY + nFs*nSc*0.5f }, 3.5f, kGreen);
    }

    // ── Info button (top-left) ────────────────────────────────────────────
    float btnR = 8.f;
    ImVec2 infoC = { x + 13.f, y + 13.f };
    bool infoHov = ImGui::IsMouseHoveringRect(
        { infoC.x-btnR, infoC.y-btnR }, { infoC.x+btnR, infoC.y+btnR });
    dl->AddCircleFilled(infoC, btnR, infoHov ? kBlurple : IM_COL32(40,42,50,200));
    dl->AddText(nullptr, 10.f, { infoC.x - 2.f, infoC.y - 5.f }, kGrey, "i");

    // ── Settings button (top-right) ───────────────────────────────────────
    bool hasSet = !mod->settings().getDefs().empty();
    ImVec2 gearC = { x + w - 13.f, y + 13.f };
    bool gearHov = hasSet && ImGui::IsMouseHoveringRect(
        { gearC.x-btnR, gearC.y-btnR }, { gearC.x+btnR, gearC.y+btnR });
    if (hasSet) {
        dl->AddCircleFilled(gearC, btnR, gearHov ? kBlurple : IM_COL32(40,42,50,200));
        // Simple gear symbol via text
        dl->AddText(nullptr, 9.f, { gearC.x - 4.f, gearC.y - 5.f }, kGrey, "*");
    }

    // ── Invisible click area ──────────────────────────────────────────────
    ImGui::SetCursorScreenPos({ x, y });
    char bid[96]; snprintf(bid,sizeof(bid),"##mc_%s",mod->getName().c_str());
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.03f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.06f });
    if (ImGui::Button(bid, { w, h })) {
        ImVec2 mp = ImGui::GetIO().MousePos;
        bool onInfo = ImGui::IsMouseHoveringRect({ infoC.x-btnR,infoC.y-btnR },{ infoC.x+btnR,infoC.y+btnR });
        bool onGear = hasSet && ImGui::IsMouseHoveringRect({ gearC.x-btnR,gearC.y-btnR },{ gearC.x+btnR,gearC.y+btnR });
        if (onInfo) {
            m_selectedMod = mod;
            m_screen      = Screen::Info;
            m_transAlpha  = 0.f;
        } else if (onGear) {
            m_selectedMod = mod;
            m_screen      = Screen::Settings;
            m_transAlpha  = 0.f;
        } else {
            mod->toggle();
        }
    }
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::TextWrapped("%s", mod->getDescription().c_str());
        ImGui::PopStyleColor();
        ImGui::EndTooltip();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  GRID screen
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderGrid() {
    renderHeader(W);
    renderCategoryBar(W);

    float gridTop = HDR_H + CAT_H;
    float gridH   = H - gridTop;
    float cardW   = (W - GRID_PAD*2 - CARD_GAP*(COLS-1)) / COLS;

    ImGui::SetCursorPos({ 0.f, gridTop });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { GRID_PAD, GRID_PAD });
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,  kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,   kv4White);
    ImGui::BeginChild("##grid", { W, gridH }, false);

    auto& mods = ModuleManager::get().getModules();
    const ModuleCategory cat = kCats[m_activeCat].cat;
    bool searching = m_searchBuf[0] != '\0';
    float scrollY  = ImGui::GetScrollY();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wPos    = ImGui::GetWindowPos();

    int  col = 0;
    float rx = GRID_PAD, ry = GRID_PAD;
    bool any = false;

    for (auto& mp : mods) {
        ModuleBase* mod = mp.get();
        bool matchCat  = searching || mod->getCategory() == cat;
        bool matchSrch = containsCI(mod->getName(), m_searchBuf) ||
                         containsCI(mod->getDescription(), m_searchBuf);
        if (!matchCat || !matchSrch) continue;
        any = true;

        float cx = wPos.x + rx;
        float cy = wPos.y + ry - scrollY;
        renderModuleCard(mod, cx, cy, cardW, CARD_H);

        col++;
        if (col >= COLS) { col=0; rx=GRID_PAD; ry+=CARD_H+CARD_GAP; }
        else rx += cardW + CARD_GAP;
    }
    if (col != 0) ry += CARD_H + CARD_GAP;
    ImGui::SetCursorPos({ 0.f, ry });

    if (!any) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        float tw = ImGui::CalcTextSize("No modules found").x;
        ImGui::SetCursorPos({ (W - tw)*0.5f, 60.f });
        ImGui::TextUnformatted("No modules found");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Back button helper
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderBackButton(float x, float y) {
    ImGui::SetCursorPos({ x, y });
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.07f,0.07f,0.09f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kv4White);
    ImGui::PushStyleColor(ImGuiCol_Text,          kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::Button("< Back##back", { 80.f, 28.f })) {
        m_screen     = Screen::Grid;
        m_transAlpha = 0.f;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SETTINGS screen
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderSettingsScreen() {
    if (!m_selectedMod) { m_screen = Screen::Grid; return; }

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();
    ModuleBase* mod = m_selectedMod;

    // Header
    dl->AddRectFilled(pos, { pos.x+W, pos.y+HDR_H }, kBg2, ROUND, ImDrawFlags_RoundCornersTop);
    dl->AddLine({ pos.x, pos.y+HDR_H }, { pos.x+W, pos.y+HDR_H }, kBorder, 1.f);

    // Module icon
    float iconSz = 30.f;
    auto* srv = IconManager::get().getIcon(mod->getIconName());
    if (srv) dl->AddImage((ImTextureID)srv, { pos.x+16.f, pos.y+11.f }, { pos.x+16.f+iconSz, pos.y+11.f+iconSz });

    // Title
    shadow(dl, 16.f, { pos.x + 16.f + iconSz + 10.f, pos.y + 14.f }, mod->getName().c_str(), kWhite);
    shadow(dl, 10.f, { pos.x + 16.f + iconSz + 10.f, pos.y + 32.f }, "Settings",            kGreyDim);

    // Close
    ImGui::SetCursorPos({ W - 42.f, 9.f });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.18f,0.18f,0.75f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.8f,0.18f,0.18f,1.f  });
    ImGui::PushStyleColor(ImGuiCol_Text,          kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::Button("X##cls2", { 30.f, 30.f })) m_open = false;
    ImGui::PopStyleVar(); ImGui::PopStyleColor(4);

    // Back button
    renderBackButton(16.f, HDR_H + 10.f);

    // Toggle enable / disable
    {
        float bx = 110.f, by = HDR_H + 10.f;
        bool en = mod->isEnabled();
        ImGui::SetCursorPos({ bx, by });
        ImGui::PushStyleColor(ImGuiCol_Button,        en ? ImVec4(0.26f,0.71f,0.5f,0.8f) : ImVec4(0.2f,0.2f,0.25f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, en ? ImVec4(0.26f,0.71f,0.5f,1.f) : ImVec4(0.3f,0.3f,0.38f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          kv4White);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
        if (ImGui::Button(en ? "Enabled##tog" : "Disabled##tog", { 90.f, 28.f }))
            mod->toggle();
        ImGui::PopStyleVar(); ImGui::PopStyleColor(3);
    }

    // Content area
    float contentY = HDR_H + 50.f;
    float contentH = H - contentY;
    ImGui::SetCursorPos({ 0.f, contentY });

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { 24.f, 16.f });
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,   5.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,   kv4White);
    ImGui::BeginChild("##settings_body", { W, contentH }, false);

    auto& defs = mod->settings().getDefs();
    if (defs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::TextWrapped("This module has no configurable settings.");
        ImGui::PopStyleColor();
    } else {
        for (auto& [key, def] : defs)
            renderSettingRow(key, const_cast<SettingDef&>(def), W - 48.f);
        // Keybind row
        ImGui::Spacing();
        ImDrawList* dl2 = ImGui::GetWindowDrawList();
        ImVec2 kp = ImGui::GetCursorScreenPos();
        dl2->AddLine(kp, { kp.x + W - 48.f, kp.y }, kBorder, 1.f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.f);
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::Text("Keybind");
        ImGui::PopStyleColor();
        ImGui::SameLine(0.f, 12.f);
        ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
        ImGui::TextUnformatted(vkName(mod->getKey()));
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  INFO screen
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderInfoScreen() {
    if (!m_selectedMod) { m_screen = Screen::Grid; return; }

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetWindowPos();
    ModuleBase* mod = m_selectedMod;

    dl->AddRectFilled(pos, { pos.x+W, pos.y+HDR_H }, kBg2, ROUND, ImDrawFlags_RoundCornersTop);
    dl->AddLine({ pos.x, pos.y+HDR_H }, { pos.x+W, pos.y+HDR_H }, kBorder, 1.f);

    float iconSz = 30.f;
    auto* srv = IconManager::get().getIcon(mod->getIconName());
    if (srv) dl->AddImage((ImTextureID)srv, { pos.x+16.f, pos.y+11.f }, { pos.x+16.f+iconSz, pos.y+11.f+iconSz });
    shadow(dl, 16.f, { pos.x+16.f+iconSz+10.f, pos.y+14.f }, mod->getName().c_str(),              kWhite);
    shadow(dl, 10.f, { pos.x+16.f+iconSz+10.f, pos.y+32.f }, categoryName(mod->getCategory()), kGreyDim);

    ImGui::SetCursorPos({ W-42.f, 9.f });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f,0.18f,0.18f,0.75f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.8f,0.18f,0.18f,1.f  });
    ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::Button("X##cli2", { 30.f, 30.f })) m_open = false;
    ImGui::PopStyleVar(); ImGui::PopStyleColor(4);

    renderBackButton(16.f, HDR_H + 10.f);

    float contentY = HDR_H + 50.f;
    ImGui::SetCursorPos({ 0.f, contentY });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 24.f, 16.f });
    ImGui::BeginChild("##info_body", { W, H - contentY }, false);

    // Large icon
    if (srv) {
        float bigSz = 64.f;
        float ix = (W - bigSz) * 0.5f - 24.f;
        ImGui::SetCursorPos({ ix, 10.f });
        ImGui::Image((ImTextureID)srv, { bigSz, bigSz });
        ImGui::Spacing(); ImGui::Spacing();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
    float tw = ImGui::CalcTextSize(mod->getName().c_str()).x;
    ImGui::SetCursorPosX((W - tw - 48.f) * 0.5f);
    ImGui::Text("%s", mod->getName().c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextWrapped("%s", mod->getDescription().c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Spacing();

    auto infoRow = [](const char* lbl, const char* val) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        ImGui::Text("%-14s", lbl);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.f, 6.f);
        ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
        ImGui::TextUnformatted(val);
        ImGui::PopStyleColor();
    };

    infoRow("Category:", categoryName(mod->getCategory()));
    infoRow("Status:",   mod->isEnabled() ? "Enabled" : "Disabled");
    infoRow("Keybind:",  vkName(mod->getKey()));
    char scBuf[16]; snprintf(scBuf,sizeof(scBuf),"%d",
        (int)mod->settings().getDefs().size());
    infoRow("Settings:", scBuf);

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Setting widget row
// ─────────────────────────────────────────────────────────────────────────────
void ModMenu::renderSettingRow(const std::string& key, SettingDef& def, float rowW) {
    ImGui::PushID(key.c_str());

    // Section divider line
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 lp = ImGui::GetCursorScreenPos();
    dl->AddLine(lp, { lp.x + rowW, lp.y }, kBorder, 0.6f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);

    // Label
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextUnformatted(def.label.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(rowW * 0.45f);
    ImGui::SetNextItemWidth(rowW * 0.55f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.06f,0.06f,0.08f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   ImVec4(0.08f,0.08f,0.12f,1.f));
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
    ImGui::PopID();
}
