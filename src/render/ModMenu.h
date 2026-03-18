#pragma once
#include "../modules/ModuleBase.h"
#include <string>

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

    void renderHeader(ImDrawList* dl, ImVec2 pos, ImVec2 sz);
    void renderTabBar(ImDrawList* dl, ImVec2 pos, ImVec2 sz);
    void renderModulesTab();
    void renderElementsTab();
    void renderEditorsTab();
    void renderInformationTab();
    void renderSettingsPanel(ModuleBase* mod);
    void renderInfoPanel(ModuleBase* mod);
    void renderSettingWidget(const std::string& key, SettingDef& def);

    bool         m_open           = false;
    int          m_activeTab      = 0;
    int          m_activeCat      = 0;
    ModuleBase*  m_selectedModule = nullptr;
    ModuleBase*  m_infoModule     = nullptr;
    bool         m_showInfo       = false;
    bool         m_capturingKey   = false;
    char         m_searchBuf[128] = {};

    // Layout constants
    static constexpr float kMenuW     = 760.f;
    static constexpr float kMenuH     = 520.f;
    static constexpr float kHeaderH   = 48.f;
    static constexpr float kTabBarH   = 42.f;
    static constexpr float kCatBarH   = 40.f;
    static constexpr float kSettingsW = 220.f;
    static constexpr float kGridPad   = 12.f;
    static constexpr float kGridGap   = 10.f;
    static constexpr float kCardH     = 148.f;
    static constexpr int   kCols      = 3;
};
