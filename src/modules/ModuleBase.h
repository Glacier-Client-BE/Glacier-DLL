#pragma once
#include "ModuleSettings.h"
#include <string>
#include <Windows.h>
#include <imgui.h>

enum class ModuleCategory { HUD, Combat, Movement, Visual, Utility };

inline const char* categoryName(ModuleCategory c) {
    switch (c) {
        case ModuleCategory::HUD:      return "HUD";
        case ModuleCategory::Combat:   return "Combat";
        case ModuleCategory::Movement: return "Movement";
        case ModuleCategory::Visual:   return "Visual";
        case ModuleCategory::Utility:  return "Utility";
    }
    return "?";
}

// ─────────────────────────────────────────────────────────────────────────────
//  ModuleBase
// ─────────────────────────────────────────────────────────────────────────────
//  iconName  — filename stem under icons/modules/ e.g. "armorhud"
//              The IconManager maps this to <dllDir>/icons/modules/armorhud.png
//              If the file doesn't exist, a procedurally generated tile is used.
// ─────────────────────────────────────────────────────────────────────────────
class ModuleBase {
public:
    ModuleBase(std::string name, std::string desc, const char* iconName,
               ModuleCategory cat,
               float defaultX = 10.f, float defaultY = 10.f,
               int defaultKey = 0)
        : m_name(std::move(name)), m_desc(std::move(desc))
        , m_icon(iconName ? iconName : "")
        , m_category(cat)
        , m_enabled(false), m_pos{defaultX, defaultY}
        , m_key(defaultKey) {}

    virtual ~ModuleBase() = default;

    virtual void onEnable()  {}
    virtual void onDisable() {}
    virtual void onTick()    {}
    virtual void onRender(ImDrawList* /*dl*/) {}
    virtual void onRenderImGui() {}

    void toggle() {
        m_enabled = !m_enabled;
        m_enabled ? onEnable() : onDisable();
    }
    void setEnabled(bool v) { if (m_enabled != v) toggle(); }

    const std::string&  getName()        const { return m_name;     }
    const std::string&  getDescription() const { return m_desc;     }
    const std::string&  getIconName()    const { return m_icon;     }
    ModuleCategory      getCategory()    const { return m_category; }
    bool                isEnabled()      const { return m_enabled;  }
    ImVec2&             getPos()               { return m_pos;      }
    ModuleSettings&     settings()             { return m_settings; }

    int  getKey() const { return m_key; }
    void setKey(int k)  { m_key = k;   }

    void pollHotkey() {
        if (m_key == 0) return;
        if (GetAsyncKeyState(m_key) & 1) toggle();
    }

protected:
    std::string    m_name, m_desc, m_icon;
    ModuleCategory m_category;
    bool           m_enabled;
    ImVec2         m_pos;
    ModuleSettings m_settings;
    int            m_key;
};
