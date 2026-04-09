#pragma once
#include "../modules/ModuleBase.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  ModMenu  ·  Glacier Client  —  Sidebar Layout
//
//   ╔══════════╦════════════════════════════════════════════╗
//   ║ SIDEBAR  ║  CONTENT AREA                              ║
//   ║──────────║────────────────────────────────────────────║
//   ║ Glacier  ║  [🔍 Search...]                [Close]     ║
//   ║ v1.x     ║──────────────────────────────────────────  ║
//   ║──────────║  ┌──────┐ ┌──────┐ ┌──────┐              ║
//   ║ ● HUD    ║  │ Mod  │ │ Mod  │ │ Mod  │              ║
//   ║ ○ Combat ║  └──────┘ └──────┘ └──────┘              ║
//   ║ ○ Move   ║  ...                                       ║
//   ║ ○ Visual ║                                            ║
//   ║ ○ Util   ║  —— or Settings sub-page ——               ║
//   ╚══════════╩════════════════════════════════════════════╝
// ─────────────────────────────────────────────────────────────────────────────

class ModMenu {
public:
    static ModMenu& get();
    void init();
    void render();
    void toggle() { m_open = !m_open; }
    bool isOpen() const { return m_open; }

private:
    ModMenu() = default;
    ModMenu(const ModMenu&) = delete;
    ModMenu& operator=(const ModMenu&) = delete;

    enum class Screen { Grid, Settings };

    void renderSidebar(ImVec2 wPos);
    void renderContentArea(ImVec2 wPos);
    void renderGridPage(ImVec2 wPos);
    void renderSettingsPage(ImVec2 wPos);

    void renderModuleCard(ModuleBase* mod, float x, float y, float w, float h);
    void renderSettingRow(const std::string& key, SettingDef& def, float rowW);
    void renderToggle(float cx, float cy, bool active);

    // ── State ────────────────────────────────────────────────────────────────
    bool        m_open        = false;
    Screen      m_screen      = Screen::Grid;
    int         m_activeCat   = 0;
    ModuleBase* m_selectedMod = nullptr;
    bool        m_capturingKey= false;
    char        m_searchBuf[128] = {};

    float m_fadeAlpha   = 1.f;
    float m_windowAlpha = 0.f;

    // sidebar highlight animation
    float m_catIndicatorY    = 0.f;
    float m_catIndicatorYTgt = 0.f;

    // ── Layout ───────────────────────────────────────────────────────────────
    static constexpr float W          = 870.f;
    static constexpr float H          = 570.f;
    static constexpr float SIDEBAR_W  = 185.f;
    static constexpr float ROUND      = 14.f;
    static constexpr float CARD_H     = 110.f;
    static constexpr float CARD_GAP   = 10.f;
    static constexpr float GRID_PAD   = 14.f;
    static constexpr int   COLS       = 3;
    static constexpr float CONTENT_X  = SIDEBAR_W;
    static constexpr float CONTENT_W  = W - SIDEBAR_W;
    static constexpr float SEARCH_H   = 52.f;
};
