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

    void renderSidebar();
    void renderModuleList();
    void renderSettingsPanel(ModuleBase* mod);
    void renderClientSettings();
    void renderSettingWidget(const std::string& key, SettingDef& def);

    bool         m_open           = false;
    int          m_activeTab      = 0;   // 0..4 = module categories, 5 = client settings
    ModuleBase*  m_selectedModule = nullptr;
    char         m_searchBuf[128] = {};

    // Keybind capture state (Client Settings tab)
    bool         m_capturingKey   = false;

    static constexpr float kMenuW     = 660.f;
    static constexpr float kMenuH     = 460.f;
    static constexpr float kSidebarW  = 130.f;
    static constexpr float kSettingsW = 210.f;

    static constexpr int kNumCategories = 5;
};
