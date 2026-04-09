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

// ════════════════════════════════════════════════════════════════════════════
//  Palette
// ════════════════════════════════════════════════════════════════════════════
static constexpr ImU32 kBlurple    = IM_COL32(114, 137, 218, 255);
static constexpr ImU32 kBlurpleDim = IM_COL32(114, 137, 218,  55);
static constexpr ImU32 kBlurpleMid = IM_COL32(114, 137, 218, 130);

static constexpr ImU32 kBg0        = IM_COL32(  8,   8,  12, 255); // deepest
static constexpr ImU32 kBg1        = IM_COL32( 12,  12,  18, 255); // sidebar
static constexpr ImU32 kBg2        = IM_COL32( 18,  18,  26, 255); // cards
static constexpr ImU32 kBg3        = IM_COL32( 24,  24,  34, 255); // hover
static constexpr ImU32 kBgContent  = IM_COL32( 14,  14,  20, 255); // content pane

static constexpr ImU32 kBorder     = IM_COL32(255, 255, 255,  12);
static constexpr ImU32 kBorderAcct = IM_COL32(114, 137, 218, 140);
static constexpr ImU32 kDivider    = IM_COL32(255, 255, 255,  18);

static constexpr ImU32 kWhite      = IM_COL32(255, 255, 255, 255);
static constexpr ImU32 kGrey       = IM_COL32(160, 165, 180, 220);
static constexpr ImU32 kGreyDim    = IM_COL32(100, 105, 120, 170);
static constexpr ImU32 kGreen      = IM_COL32( 67, 181, 129, 255);
static constexpr ImU32 kRed        = IM_COL32(220,  60,  60, 255);
static constexpr ImU32 kYellow     = IM_COL32(240, 190,  50, 255);
static constexpr ImU32 kShadow     = IM_COL32(  0,   0,   0, 160);
static constexpr ImU32 kShadowHvy  = IM_COL32(  0,   0,   0, 210);

// ImVec4 versions for PushStyleColor
static constexpr ImVec4 kv4Blurple { 0.447f, 0.537f, 0.855f, 1.f };
static constexpr ImVec4 kv4Grey    { 0.627f, 0.647f, 0.706f, 0.86f };
static constexpr ImVec4 kv4GreyDim { 0.392f, 0.412f, 0.471f, 0.667f }; // ← FIXED
static constexpr ImVec4 kv4White   { 1.f,    1.f,    1.f,    1.f };
static constexpr ImVec4 kv4Green   { 0.263f, 0.710f, 0.506f, 1.f };
static constexpr ImVec4 kv4Red     { 0.863f, 0.235f, 0.235f, 1.f };

// ════════════════════════════════════════════════════════════════════════════
//  Category descriptors
// ════════════════════════════════════════════════════════════════════════════
struct CatDesc { const char* label; const char* icon; ModuleCategory cat; };
static constexpr CatDesc kCats[] = {
    { "HUD",      "HUD", ModuleCategory::HUD      },
    { "Combat",   "CMB", ModuleCategory::Combat   },
    { "Movement", "MOV", ModuleCategory::Movement },
    { "Visual",   "VIS", ModuleCategory::Visual   },
    { "Utility",  "UTL", ModuleCategory::Utility  },
};
static constexpr int kCatCount = 5;

// ════════════════════════════════════════════════════════════════════════════
//  Helpers
// ════════════════════════════════════════════════════════════════════════════
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
    if (vk >= 'A' && vk <= 'Z') { buf[0] = (char)vk; buf[1] = 0; return buf; }
    if (vk >= '0' && vk <= '9') { buf[0] = (char)vk; buf[1] = 0; return buf; }
    if (vk >= VK_F1 && vk <= VK_F12) { snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1); return buf; }
    switch (vk) {
        case VK_INSERT: return "INS"; case VK_DELETE: return "DEL";
        case VK_HOME:   return "HOME"; case VK_END:   return "END";
        case VK_SPACE:  return "SPC";  case VK_RETURN: return "RET";
        case VK_SHIFT:  return "SHIFT"; case VK_CONTROL: return "CTRL";
        case VK_MENU:   return "ALT";
    }
    snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

// Drop shadow text draw
static void shadowText(ImDrawList* dl, float sz, ImVec2 p, ImU32 col, const char* t) {
    dl->AddText(nullptr, sz, { p.x + 1.3f, p.y + 1.3f }, kShadow, t);
    dl->AddText(nullptr, sz, p, col, t);
}

// Lerp ImU32 alpha
static ImU32 withAlpha(ImU32 col, float a) {
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(col);
    v.w *= a;
    return ImGui::ColorConvertFloat4ToU32(v);
}

// ════════════════════════════════════════════════════════════════════════════
//  Singleton
// ════════════════════════════════════════════════════════════════════════════
ModMenu& ModMenu::get() { static ModMenu i; return i; }
void ModMenu::init() {}

// ════════════════════════════════════════════════════════════════════════════
//  render()  —  top-level entry point called every frame
// ════════════════════════════════════════════════════════════════════════════
void ModMenu::render() {
    if (!m_open) {
        m_windowAlpha = 0.f;
        return;
    }

    // ── Key capture mode ─────────────────────────────────────────────────
    if (m_capturingKey) {
        for (int vk = 8; vk <= 0xFE; ++vk) {
            if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
            if (GetAsyncKeyState(vk) & 0x8001) {
                if (m_selectedMod) m_selectedMod->setKey(vk);
                m_capturingKey = false;
                break;
            }
        }
    }

    // ── Open/close fade animation ────────────────────────────────────────
    m_windowAlpha += (1.f - m_windowAlpha) * 0.22f;

    // ── Sidebar category indicator lerp ─────────────────────────────────
    // Calculated once in renderSidebar; lerp here so it's smooth across frames
    m_catIndicatorY += (m_catIndicatorYTgt - m_catIndicatorY) * 0.20f;

    // ── Page transition fade ─────────────────────────────────────────────
    m_fadeAlpha += (1.f - m_fadeAlpha) * 0.20f;

    // ── ImGui window setup ───────────────────────────────────────────────
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = { (io.DisplaySize.x - W) * 0.5f, (io.DisplaySize.y - H) * 0.5f };

    ImGui::SetNextWindowPos(center, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ W, H }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   { 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  ROUND);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    constexpr ImGuiWindowFlags WF =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("##glacier_menu", &m_open, WF)) {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetWindowPos();

        // ── Drop shadow layers ───────────────────────────────────────────
        for (int i = 7; i >= 1; --i) {
            float fi = (float)i;
            dl->AddRectFilled(
                { pos.x + fi, pos.y + fi },
                { pos.x + W + fi, pos.y + H + fi },
                IM_COL32(0, 0, 0, (int)(16 * m_windowAlpha)),
                ROUND + 3.f);
        }

        // ── Main window background ───────────────────────────────────────
        dl->AddRectFilled(pos, { pos.x + W, pos.y + H }, kBg0, ROUND);

        // ── Sidebar background ───────────────────────────────────────────
        dl->AddRectFilled(pos, { pos.x + SIDEBAR_W, pos.y + H },
                          kBg1, ROUND, ImDrawFlags_RoundCornersLeft);

        // ── Sidebar/content divider ──────────────────────────────────────
        dl->AddLine({ pos.x + SIDEBAR_W, pos.y + 16.f },
                    { pos.x + SIDEBAR_W, pos.y + H - 16.f },
                    kDivider, 1.f);

        // ── Outer border ────────────────────────────────────────────────
        dl->AddRect(pos, { pos.x + W, pos.y + H },
                    withAlpha(kBorder, m_windowAlpha), ROUND, 0, 1.f);

        // ── Render panels ────────────────────────────────────────────────
        renderSidebar(pos);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_fadeAlpha);
        renderContentArea(pos);
        ImGui::PopStyleVar();
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// ════════════════════════════════════════════════════════════════════════════
//  Sidebar
// ════════════════════════════════════════════════════════════════════════════
void ModMenu::renderSidebar(ImVec2 pos) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Logo block ───────────────────────────────────────────────────────
    float logoY = pos.y + 22.f;
    shadowText(dl, 18.f, { pos.x + 20.f, logoY }, kBlurple, "GLACIER");

    {
        ImVec2 vts = ImGui::CalcTextSize(GLACIER_VERSION);
        float  vsx = 18.f / ImGui::GetFontSize();
        float  vw  = vts.x * vsx;
        // "v" prefix + version small text
        shadowText(dl, 12.f,
            { pos.x + 20.f + ImGui::CalcTextSize("GLACIER").x * 18.f / ImGui::GetFontSize() + 7.f,
              logoY + 4.f },
            kGreyDim, "v" GLACIER_VERSION);
    }

    // Thin horizontal rule under logo
    float ruleY = pos.y + 52.f;
    dl->AddLine({ pos.x + 14.f, ruleY }, { pos.x + SIDEBAR_W - 14.f, ruleY },
                kDivider, 1.f);

    // ── Category list ────────────────────────────────────────────────────
    // Count modules per category for the badge
    auto& mods = ModuleManager::get().getModules();
    int catCount[kCatCount] = {};
    for (auto& m : mods) {
        for (int i = 0; i < kCatCount; ++i)
            if (m->getCategory() == kCats[i].cat) { catCount[i]++; break; }
    }

    float catStartY = pos.y + 64.f;
    float catItemH  = 40.f;

    // Update the target Y for the sliding indicator
    m_catIndicatorYTgt = catStartY + m_activeCat * catItemH + catItemH * 0.5f - 16.f;

    // Draw sliding indicator bar (left side glow pill)
    {
        float iy  = m_catIndicatorY;
        float ih  = 32.f;
        // Glow
        dl->AddRectFilled({ pos.x + 2.f, iy },
                          { pos.x + 5.f, iy + ih },
                          withAlpha(kBlurple, 0.5f), 3.f);
        // Solid indicator
        dl->AddRectFilled({ pos.x + 3.f, iy + 4.f },
                          { pos.x + 5.f, iy + ih - 4.f },
                          kBlurple, 3.f);
    }

    for (int i = 0; i < kCatCount; ++i) {
        bool    active = (m_activeCat == i && m_screen == Screen::Grid);
        float   iy     = catStartY + i * catItemH;
        ImVec2  pMin   = { pos.x + 10.f, iy };
        ImVec2  pMax   = { pos.x + SIDEBAR_W - 8.f, iy + catItemH - 2.f };

        bool hovered = ImGui::IsMouseHoveringRect(pMin, pMax);

        // Row background
        if (active) {
            dl->AddRectFilled(pMin, pMax, kBlurpleDim, 8.f);
            dl->AddRect(pMin, pMax, withAlpha(kBlurple, 0.35f), 8.f, 0, 1.f);
        } else if (hovered) {
            dl->AddRectFilled(pMin, pMax, kBg3, 8.f);
        }

        // Icon badge (small coloured square)
        float iconX = pMin.x + 10.f;
        float iconY = iy + (catItemH - 2.f) * 0.5f - 10.f;
        ImU32 iconBg = active ? kBlurple : (hovered ? IM_COL32(50, 55, 70, 200) : IM_COL32(35, 38, 50, 180));
        dl->AddRectFilled({ iconX, iconY }, { iconX + 20.f, iconY + 20.f }, iconBg, 5.f);
        // Icon text (3-letter abbreviation)
        ImVec2 iTs = ImGui::CalcTextSize(kCats[i].icon);
        float  iFs = 9.f;
        float  iSc = iFs / ImGui::GetFontSize();
        dl->AddText(nullptr, iFs,
            { iconX + (20.f - iTs.x * iSc) * 0.5f, iconY + (20.f - iTs.y * iSc) * 0.5f },
            kWhite, kCats[i].icon);

        // Category name
        float  tfs = 13.f;
        ImU32  tc  = active ? kWhite : (hovered ? IM_COL32(200, 205, 220, 255) : kGrey);
        shadowText(dl, tfs, { iconX + 26.f, iy + (catItemH - 2.f) * 0.5f - tfs * 0.5f + 1.f },
                   tc, kCats[i].label);

        // Module count badge
        char countBuf[8]; snprintf(countBuf, sizeof(countBuf), "%d", catCount[i]);
        ImVec2 cbTs = ImGui::CalcTextSize(countBuf);
        float  cbFs = 10.f;
        float  cbSc = cbFs / ImGui::GetFontSize();
        float  cbW  = cbTs.x * cbSc + 10.f;
        float  cbX  = pMax.x - cbW - 4.f;
        float  cbY  = iy + (catItemH - 2.f) * 0.5f - 9.f;
        ImU32  cbBg = active ? withAlpha(kBlurple, 0.6f) : IM_COL32(30, 32, 42, 200);
        dl->AddRectFilled({ cbX, cbY }, { cbX + cbW, cbY + 18.f }, cbBg, 9.f);
        dl->AddText(nullptr, cbFs,
            { cbX + (cbW - cbTs.x * cbSc) * 0.5f, cbY + (18.f - cbTs.y * cbSc) * 0.5f },
            active ? kWhite : kGreyDim, countBuf);

        // Invisible click region
        ImGui::SetCursorScreenPos(pMin);
        char bid[16]; snprintf(bid, sizeof(bid), "##scat%d", i);
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0,0,0,0 });
        if (ImGui::Button(bid, { pMax.x - pMin.x, pMax.y - pMin.y })) {
            if (m_activeCat != i || m_screen != Screen::Grid) {
                m_activeCat = i;
                m_screen    = Screen::Grid;
                m_fadeAlpha = 0.f;
            }
        }
        ImGui::PopStyleColor(3);
    }

    // ── Bottom: enabled-count info ───────────────────────────────────────
    int enabledCount = 0;
    for (auto& m : mods) if (m->isEnabled()) enabledCount++;

    float bottomY = pos.y + H - 36.f;
    dl->AddLine({ pos.x + 14.f, bottomY - 4.f },
                { pos.x + SIDEBAR_W - 14.f, bottomY - 4.f }, kDivider, 1.f);

    char enBuf[48]; snprintf(enBuf, sizeof(enBuf), "%d enabled", enabledCount);
    ImVec2 enTs = ImGui::CalcTextSize(enBuf);
    float  enSc = 11.f / ImGui::GetFontSize();
    dl->AddText(nullptr, 11.f,
        { pos.x + SIDEBAR_W * 0.5f - enTs.x * enSc * 0.5f, bottomY + 4.f },
        kGreyDim, enBuf);
}

// ════════════════════════════════════════════════════════════════════════════
//  Content area — dispatches to Grid or Settings
// ════════════════════════════════════════════════════════════════════════════
void ModMenu::renderContentArea(ImVec2 pos) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float cx = pos.x + CONTENT_X;
    float cw = CONTENT_W;

    // ── Top bar: search + close ──────────────────────────────────────────
    float barY = pos.y;

    // Search box
    float searchX = cx + 16.f;
    float searchW = 220.f;
    float searchY = barY + (SEARCH_H - 28.f) * 0.5f;

    // Search background pill
    dl->AddRectFilled({ searchX - 4.f, searchY - 2.f },
                      { searchX + searchW + 4.f, searchY + 30.f },
                      kBg3, 10.f);
    dl->AddRect({ searchX - 4.f, searchY - 2.f },
                { searchX + searchW + 4.f, searchY + 30.f },
                kBorder, 10.f, 0, 1.f);

    // Search icon (S)
    dl->AddText(nullptr, 11.f, { searchX + 4.f, searchY + 9.f }, kGreyDim, "S:");

    ImGui::SetCursorScreenPos({ searchX + 20.f, searchY + 4.f });
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,{ 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_Text,           kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::SetNextItemWidth(searchW - 20.f);
    if (ImGui::InputTextWithHint("##gsearch", "Search modules...",
                                  m_searchBuf, sizeof(m_searchBuf))) {
        if (m_screen != Screen::Grid) { m_screen = Screen::Grid; m_fadeAlpha = 0.f; }
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    // Page title (right side of search bar)
    const char* pageTitle = (m_screen == Screen::Settings && m_selectedMod)
        ? m_selectedMod->getName().c_str()
        : kCats[m_activeCat].label;
    ImVec2 ptTs = ImGui::CalcTextSize(pageTitle);
    float  ptFs = 16.f;
    float  ptSc = ptFs / ImGui::GetFontSize();
    shadowText(dl, ptFs,
        { cx + cw * 0.5f - ptTs.x * ptSc * 0.5f, barY + (SEARCH_H - ptFs) * 0.5f },
        kWhite, pageTitle);

    // Close button
    float closeX = pos.x + W - 42.f;
    float closeY = barY + (SEARCH_H - 28.f) * 0.5f;
    ImGui::SetCursorScreenPos({ closeX, closeY });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8f, 0.15f, 0.15f, 0.7f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.9f, 0.15f, 0.15f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_Text,           kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::Button("X##gclose", { 28.f, 28.f })) m_open = false;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // Separator under top bar
    float sepY = pos.y + SEARCH_H;
    dl->AddLine({ cx, sepY }, { cx + cw, sepY }, kDivider, 1.f);

    // ── Dispatch page ────────────────────────────────────────────────────
    if (m_screen == Screen::Settings)
        renderSettingsPage(pos);
    else
        renderGridPage(pos);
}

// ════════════════════════════════════════════════════════════════════════════
//  Toggle switch widget
// ════════════════════════════════════════════════════════════════════════════
void ModMenu::renderToggle(float cx, float cy, bool active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    constexpr float W  = 34.f, H  = 18.f, R  = 9.f;
    constexpr float KR = 7.f;

    ImU32 track = active ? kBlurple : IM_COL32(42, 44, 56, 255);
    dl->AddRectFilled({ cx, cy }, { cx + W, cy + H }, track, R);
    dl->AddRect({ cx, cy }, { cx + W, cy + H }, withAlpha(kWhite, 0.08f), R, 0, 1.f);

    float kx = active ? (cx + W - KR - 2.f) : (cx + KR + 2.f);
    dl->AddCircleFilled({ kx, cy + H * 0.5f }, KR, kWhite);

    // Inner shadow on knob
    dl->AddCircle({ kx, cy + H * 0.5f }, KR, IM_COL32(0, 0, 0, 40), 0, 1.2f);
}

// ════════════════════════════════════════════════════════════════════════════
//  Module card
// ════════════════════════════════════════════════════════════════════════════
void ModMenu::renderModuleCard(ModuleBase* mod, float x, float y, float w, float h) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    bool        en  = mod->isEnabled();

    ImRect  bb({ x, y }, { x + w, y + h });
    bool    hov = ImGui::IsMouseHoveringRect(bb.Min, bb.Max);

    // ── Card background ──────────────────────────────────────────────────
    ImU32 cardBg = en ? IM_COL32(20, 22, 34, 255)
                      : (hov ? kBg3 : kBg2);
    dl->AddRectFilled(bb.Min, bb.Max, cardBg, 10.f);

    // Enabled accent top-edge gradient
    if (en) {
        dl->AddRectFilledMultiColor(
            { x, y }, { x + w, y + 3.f },
            kBlurple, kBlurple,
            IM_COL32(114, 137, 218, 0), IM_COL32(114, 137, 218, 0));
    }

    // Card border
    ImU32 border = en ? IM_COL32(114, 137, 218, 100)
                      : (hov ? IM_COL32(255, 255, 255, 22) : kBorder);
    dl->AddRect(bb.Min, bb.Max, border, 10.f, 0, 1.f);

    // ── Module icon ──────────────────────────────────────────────────────
    constexpr float PAD    = 12.f;
    constexpr float ICON_S = 36.f;

    auto* srv = IconManager::get().getIcon(mod->getIconName());
    if (srv) {
        ImU32 tint = en ? kWhite : IM_COL32(120, 125, 145, 200);
        dl->AddImage((ImTextureID)srv,
            { x + PAD, y + PAD }, { x + PAD + ICON_S, y + PAD + ICON_S },
            { 0, 0 }, { 1, 1 }, tint);
    } else {
        // Fallback tile using first letter
        ImU32 tileBg  = en ? kBlurpleDim : IM_COL32(30, 32, 42, 200);
        ImU32 tileBdr = en ? kBlurpleMid : IM_COL32(50, 54, 68, 200);
        dl->AddRectFilled({ x + PAD, y + PAD },
                          { x + PAD + ICON_S, y + PAD + ICON_S },
                          tileBg, 7.f);
        dl->AddRect({ x + PAD, y + PAD },
                    { x + PAD + ICON_S, y + PAD + ICON_S },
                    tileBdr, 7.f, 0, 1.f);
        char lb[2] = { (char)::toupper((unsigned char)mod->getName()[0]), 0 };
        ImVec2 lTs = ImGui::CalcTextSize(lb);
        float  lFs = 16.f;
        float  lSc = lFs / ImGui::GetFontSize();
        dl->AddText(nullptr, lFs,
            { x + PAD + (ICON_S - lTs.x * lSc) * 0.5f,
              y + PAD + (ICON_S - lTs.y * lSc) * 0.5f },
            en ? kBlurple : kGreyDim, lb);
    }

    // ── Toggle switch (top-right) ────────────────────────────────────────
    constexpr float TOG_W = 34.f, TOG_H = 18.f;
    float togX = x + w - PAD - TOG_W;
    float togY = y + PAD;
    ImRect togBb({ togX, togY }, { togX + TOG_W, togY + TOG_H });
    bool   togHov = ImGui::IsMouseHoveringRect(togBb.Min, togBb.Max);

    renderToggle(togX, togY, en);

    ImGui::SetCursorScreenPos(togBb.Min);
    char tid[72]; snprintf(tid, sizeof(tid), "##tog_%s", mod->getName().c_str());
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0,0,0,0.08f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0,0,0,0.16f });
    if (ImGui::InvisibleButton(tid, { TOG_W, TOG_H })) mod->toggle();
    ImGui::PopStyleColor(3);

    // ── Module name + description ────────────────────────────────────────
    float textY = y + PAD + ICON_S + 9.f;
    shadowText(dl, 14.f, { x + PAD, textY },
               en ? kWhite : kGrey, mod->getName().c_str());

    std::string desc = mod->getDescription();
    if ((int)desc.size() > 44) desc = desc.substr(0, 41) + "...";
    dl->AddText(nullptr, 11.f, { x + PAD, textY + 18.f },
                kGreyDim, desc.c_str());

    // ── Hotkey badge ─────────────────────────────────────────────────────
    if (mod->getKey() != 0) {
        const char* kn = vkName(mod->getKey());
        ImVec2 knTs = ImGui::CalcTextSize(kn);
        float  knFs = 9.f;
        float  knSc = knFs / ImGui::GetFontSize();
        float  knW  = knTs.x * knSc + 8.f;
        float  knX  = x + PAD;
        float  knY  = y + h - 16.f;
        dl->AddRectFilled({ knX, knY }, { knX + knW, knY + 12.f },
                          IM_COL32(30, 32, 42, 200), 3.f);
        dl->AddRect({ knX, knY }, { knX + knW, knY + 12.f },
                    withAlpha(kBlurple, 0.5f), 3.f, 0, 1.f);
        dl->AddText(nullptr, knFs,
            { knX + (knW - knTs.x * knSc) * 0.5f, knY + 1.5f },
            kGreyDim, kn);
    }

    // ── Card click (opens settings page) ────────────────────────────────
    ImGui::SetCursorScreenPos(bb.Min);
    char cid[72]; snprintf(cid, sizeof(cid), "##card_%s", mod->getName().c_str());
    ImGui::InvisibleButton(cid, { w, h });
    if (ImGui::IsItemClicked(0) && !togHov) {
        m_selectedMod = mod;
        m_screen      = Screen::Settings;
        m_fadeAlpha   = 0.f;
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  Grid page
// ════════════════════════════════════════════════════════════════════════════
void ModMenu::renderGridPage(ImVec2 pos) {
    float cx      = pos.x + CONTENT_X;
    float contentH = H - SEARCH_H;
    float cardW   = (CONTENT_W - GRID_PAD * 2.f - CARD_GAP * (COLS - 1)) / COLS;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { GRID_PAD, GRID_PAD });
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,  kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,   kv4White);

    ImGui::SetCursorPos({ CONTENT_X, SEARCH_H });
    ImGui::BeginChild("##glaciergrid", { CONTENT_W, contentH }, false,
                      ImGuiWindowFlags_NoScrollbar == 0 ? 0 : ImGuiWindowFlags_NoScrollbar);

    auto& mods       = ModuleManager::get().getModules();
    ModuleCategory   cat      = kCats[m_activeCat].cat;
    bool             searching = m_searchBuf[0] != '\0';
    float            scrollY  = ImGui::GetScrollY();
    ImVec2           wPos     = ImGui::GetWindowPos();

    int   col = 0;
    float rx  = GRID_PAD, ry = GRID_PAD;
    bool  any = false;

    for (auto& mp : mods) {
        ModuleBase* mod = mp.get();
        bool matchCat  = mod->getCategory() == cat;
        bool matchSrch = containsCI(mod->getName(), m_searchBuf) ||
                         containsCI(mod->getDescription(), m_searchBuf);

        if (searching && !matchSrch) continue;
        if (!searching && !matchCat) continue;

        any = true;
        float cardX = wPos.x + rx;
        float cardY = wPos.y + ry - scrollY;

        renderModuleCard(mod, cardX, cardY, cardW, CARD_H);

        col++;
        if (col >= COLS) { col = 0; rx = GRID_PAD; ry += CARD_H + CARD_GAP; }
        else rx += cardW + CARD_GAP;
    }
    if (col != 0) ry += CARD_H + CARD_GAP;

    // Expand child height so scrollbar works
    ImGui::SetCursorPos({ 0.f, ry });

    if (!any) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
        float tw = ImGui::CalcTextSize("No modules found.").x;
        ImGui::SetCursorPos({ (CONTENT_W - tw) * 0.5f, 70.f });
        ImGui::TextUnformatted("No modules found.");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

// ════════════════════════════════════════════════════════════════════════════
//  Settings page
// ════════════════════════════════════════════════════════════════════════════
void ModMenu::renderSettingsPage(ImVec2 pos) {
    if (!m_selectedMod) { m_screen = Screen::Grid; return; }

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ModuleBase* mod  = m_selectedMod;
    bool        en   = mod->isEnabled();
    float       contentH = H - SEARCH_H;

    // Subtle tinted background in content area
    dl->AddRectFilled(
        { pos.x + CONTENT_X, pos.y + SEARCH_H },
        { pos.x + W,         pos.y + H },
        kBgContent, ROUND, ImDrawFlags_RoundCornersBottomRight);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 22.f, 18.f });
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          { 0,0,0,0 });
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,         kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,   kv4White);

    ImGui::SetCursorPos({ CONTENT_X, SEARCH_H });
    ImGui::BeginChild("##glaciersettings", { CONTENT_W, contentH }, false);

    // ── Back button ──────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.11f, 0.11f, 0.16f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kv4White);
    ImGui::PushStyleColor(ImGuiCol_Text,           kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.f);
    if (ImGui::Button("< Back", { 78.f, 30.f })) {
        m_screen    = Screen::Grid;
        m_fadeAlpha = 0.f;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // ── Module header ────────────────────────────────────────────────────
    ImGui::SameLine(0.f, 16.f);

    // Icon
    auto* srv = IconManager::get().getIcon(mod->getIconName());
    if (srv) {
        ImGui::Image((ImTextureID)srv, { 36.f, 36.f });
        ImGui::SameLine(0.f, 12.f);
    }

    ImGui::BeginGroup();

    // Module name large
    ImGui::PushStyleColor(ImGuiCol_Text, kv4White);
    ImGui::SetWindowFontScale(1.22f);
    ImGui::TextUnformatted(mod->getName().c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    // Description
    ImGui::PushStyleColor(ImGuiCol_Text, kv4GreyDim);  // ← kv4GreyDim used correctly
    ImGui::TextUnformatted(mod->getDescription().c_str());
    ImGui::PopStyleColor();

    ImGui::EndGroup();

    // Master toggle (right-aligned)
    float  togX = CONTENT_W - 22.f - 34.f;
    ImVec2 togScreenPos = ImGui::GetCursorScreenPos();
    // Rewind to same Y as back button
    ImGui::SetCursorPos({ togX, 18.f + (30.f - 18.f) * 0.5f });
    ImVec2 tp = ImGui::GetCursorScreenPos();
    renderToggle(tp.x, tp.y, en);
    if (ImGui::InvisibleButton("##mastertog", { 34.f, 18.f })) mod->toggle();

    ImGui::Spacing(); ImGui::Spacing();

    // ── Separator ────────────────────────────────────────────────────────
    {
        ImVec2 lp = ImGui::GetCursorScreenPos();
        dl->AddLine(lp, { lp.x + CONTENT_W - 44.f, lp.y }, kDivider, 1.f);
        ImGui::Dummy({ 0.f, 10.f });
    }

    // ── Settings section header ──────────────────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4GreyDim);
        ImGui::TextUnformatted("SETTINGS");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── Settings list ────────────────────────────────────────────────────
    auto& defs = mod->settings().getDefs();
    if (defs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kv4GreyDim);
        ImGui::TextWrapped("This module has no configurable settings.");
        ImGui::PopStyleColor();
    } else {
        for (auto& [key, def] : defs) {
            renderSettingRow(key, const_cast<SettingDef&>(def), CONTENT_W - 44.f);
        }
    }

    // ── Keybind section ──────────────────────────────────────────────────
    ImGui::Spacing();
    {
        ImVec2 lp = ImGui::GetCursorScreenPos();
        dl->AddLine(lp, { lp.x + CONTENT_W - 44.f, lp.y }, kDivider, 1.f);
        ImGui::Dummy({ 0.f, 8.f });
    }

    ImGui::PushStyleColor(ImGuiCol_Text, kv4GreyDim);
    ImGui::TextUnformatted("KEYBIND");
    ImGui::PopStyleColor();
    ImGui::SameLine(CONTENT_W - 44.f - 72.f);

    // Keybind button
    ImGui::PushStyleColor(ImGuiCol_Button,
        m_capturingKey ? ImVec4(0.447f, 0.537f, 0.855f, 0.45f)
                       : ImVec4(0.11f, 0.11f, 0.17f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.21f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_Text,           kv4White);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);

    const char* kbLabel = m_capturingKey ? "Press key..." : vkName(mod->getKey());
    char kbid[64]; snprintf(kbid, sizeof(kbid), "%s##kb", kbLabel);
    if (ImGui::Button(kbid, { 72.f, 24.f })) {
        m_capturingKey = true;
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // Clear keybind button
    if (mod->getKey() != 0) {
        ImGui::SameLine(0.f, 6.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.11f, 0.11f, 0.17f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kv4Red);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kv4Red);
        ImGui::PushStyleColor(ImGuiCol_Text,           kv4White);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        if (ImGui::Button("CLR##kbclr", { 36.f, 24.f })) {
            mod->setKey(0);
            m_capturingKey = false;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

// ════════════════════════════════════════════════════════════════════════════
//  Setting row widget
// ════════════════════════════════════════════════════════════════════════════
void ModMenu::renderSettingRow(const std::string& key, SettingDef& def, float rowW) {
    ImGui::PushID(key.c_str());

    // Row background (subtle alternating)
    ImVec2 rMin = ImGui::GetCursorScreenPos();
    ImVec2 rMax = { rMin.x + rowW, rMin.y + 28.f };
    if (ImGui::IsMouseHoveringRect(rMin, { rMax.x, rMax.y + 6.f }))
        ImGui::GetWindowDrawList()->AddRectFilled(rMin, { rMax.x, rMax.y + 2.f },
                                                  IM_COL32(255, 255, 255, 5), 4.f);

    // Label
    ImGui::PushStyleColor(ImGuiCol_Text, kv4Grey);
    ImGui::TextUnformatted(def.label.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(rowW * 0.48f);
    ImGui::SetNextItemWidth(rowW * 0.52f);

    // Widgets
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  { 6.f, 3.f });
    ImGui::PushStyleColor(ImGuiCol_FrameBg,           ImVec4(0.07f, 0.07f, 0.10f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,    ImVec4(0.10f, 0.10f, 0.14f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,     ImVec4(0.12f, 0.12f, 0.17f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,         kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,   kv4White);
    ImGui::PushStyleColor(ImGuiCol_CheckMark,          kv4Blurple);
    ImGui::PushStyleColor(ImGuiCol_Border,             ImVec4(0.447f, 0.537f, 0.855f, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_Text,               kv4White);

    char wid[80]; snprintf(wid, sizeof(wid), "##sv_%s", key.c_str());

    if (std::holds_alternative<bool>(def.value)) {
        bool v = std::get<bool>(def.value);
        if (ImGui::Checkbox(wid, &v)) def.value = v;
    }
    else if (std::holds_alternative<int>(def.value)) {
        int v  = std::get<int>(def.value);
        int mn = std::get<int>(def.minVal);
        int mx = std::get<int>(def.maxVal);
        if (ImGui::SliderInt(wid, &v, mn, mx)) def.value = v;
    }
    else if (std::holds_alternative<float>(def.value)) {
        float v  = std::get<float>(def.value);
        float mn = std::get<float>(def.minVal);
        float mx = std::get<float>(def.maxVal);
        if (ImGui::SliderFloat(wid, &v, mn, mx, "%.2f")) def.value = v;
    }
    else if (std::holds_alternative<std::string>(def.value)) {
        auto& sv = std::get<std::string>(def.value);
        char buf[256] = {};
        strncpy_s(buf, sv.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText(wid, buf, sizeof(buf))) sv = buf;
    }

    ImGui::PopStyleColor(8);
    ImGui::PopStyleVar(2);

    ImGui::Spacing();

    // Thin sub-divider
    ImVec2 dlP = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(dlP, { dlP.x + rowW, dlP.y },
                                        IM_COL32(255, 255, 255, 10), 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 7.f);

    ImGui::PopID();
}
