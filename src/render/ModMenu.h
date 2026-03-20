#pragma once
#include "../modules/ModuleBase.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  ModMenu  —  three-state machine
//
//  GRID     : 3-col module card grid with category pills + search
//  SETTINGS : dedicated per-module settings screen with ← Back
//  INFO     : per-module description / info screen with ← Back
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

    enum class Screen { Grid, Settings, Info };

    // Sub-renders
    void renderGrid();
    void renderSettingsScreen();
    void renderInfoScreen();

    // Helpers
    void renderHeader(float panW);
    void renderCategoryBar(float panW);
    void renderModuleCard(ModuleBase* mod, float x, float y, float w, float h);
    void renderBackButton(float x, float y);
    void renderSettingRow(const std::string& key, SettingDef& def, float panW);
    void drawBg(float w, float h);

    bool        m_open           = false;
    Screen      m_screen         = Screen::Grid;
    int         m_activeCat      = 0;
    ModuleBase* m_selectedMod    = nullptr;
    bool        m_capturingKey   = false;
    char        m_searchBuf[128] = {};

    // Animation: fade-in alpha for screen transitions
    float m_transAlpha = 1.f;

    // Dimensions
    static constexpr float W        = 780.f;
    static constexpr float H        = 540.f;
    static constexpr float HDR_H    = 52.f;
    static constexpr float CAT_H    = 44.f;
    static constexpr float ROUND    = 14.f;
    static constexpr float CARD_H   = 152.f;
    static constexpr float CARD_GAP = 10.f;
    static constexpr float GRID_PAD = 14.f;
    static constexpr int   COLS     = 3;
};
