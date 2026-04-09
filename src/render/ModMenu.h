#pragma once
#include "../modules/ModuleBase.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  ModMenu
//
//  Redesigned to feature a persistent Top Bar (Logo + Category Tabs)
//  with the remaining window body dynamically rendering either:
//  - GRID: Scrolled view of module cards.
//  - SETTINGS: Dedicated page embedded for a specific module's settings.
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

    // Layout
    void renderHeaderAndTabs(float panW);
    void renderGridSubPage();
    void renderSettingsSubPage();

    // Elements
    void renderModuleCard(ModuleBase* mod, float x, float y, float w, float h);
    void renderSettingRow(const std::string& key, SettingDef& def, float panW);
    void renderToggleSwitch(float x, float y, bool active);

    bool        m_open           = false;
    Screen      m_screen         = Screen::Grid;
    int         m_activeCat      = 0;
    ModuleBase* m_selectedMod    = nullptr;
    bool        m_capturingKey   = false;
    char        m_searchBuf[128] = {};

    // Animation: fade-in alpha for screen transitions
    float m_transAlpha = 1.f;
    float m_scrollAnim = 0.f;

    // Dimensions
    static constexpr float W        = 800.f;
    static constexpr float H        = 560.f;
    static constexpr float HDR_H    = 60.f;
    static constexpr float CAT_H    = 50.f;
    static constexpr float ROUND    = 12.f;
    static constexpr float CARD_H   = 120.f;
    static constexpr float CARD_GAP = 12.f;
    static constexpr float GRID_PAD = 16.f;
    static constexpr int   COLS     = 3;
};
