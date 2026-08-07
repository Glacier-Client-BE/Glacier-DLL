#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Setting.h"

// Base class for every feature ("module"). Modules are pure logic: they expose
// settings and react to enable/disable + per-frame ticks. They know nothing
// about the UI or about how they were registered.
//
// HUD-drawing modules derive from HudModule (Phase 2), which layers position/
// anchor/scale settings on top of this. Deliberately absent here: any hook into
// UI serialization — the native menu reads Module/Setting directly, in-process,
// so no bridge or reflection layer is needed.
namespace glacier {

enum class Category { Combat, Movement, Visual, Player, World, Misc };

class Module {
public:
    Module(std::string name, std::string description, Category category, int keybind = 0)
        : m_name(std::move(name)), m_description(std::move(description)),
          m_category(category), m_keybind(keybind) {}

    virtual ~Module() = default;

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    // Lifecycle — overridden by concrete modules.
    virtual void onEnable()  {}
    virtual void onDisable() {}
    virtual void onTick()    {}                   // once per client tick
    virtual void onRender()  {}                   // once per Present
    virtual void onClick(bool /*rightButton*/) {} // mouse press

    void setEnabled(bool enabled) {
        if (m_enabled == enabled) return;
        m_enabled = enabled;
        if (enabled) onEnable();
        else         onDisable();
    }
    void toggle() { setEnabled(!m_enabled); }
    bool enabled() const { return m_enabled; }

    const std::string& name() const { return m_name; }
    const std::string& description() const { return m_description; }
    Category category() const { return m_category; }
    int  keybind() const { return m_keybind; }
    void setKeybind(int vk) { m_keybind = vk; }

    std::vector<Setting>& settings() { return m_settings; }
    const std::vector<Setting>& settings() const { return m_settings; }

    Setting* setting(std::string_view id) {
        for (auto& s : m_settings) {
            if (s.id() == id) return &s;
        }
        return nullptr;
    }

    const Setting* setting(std::string_view id) const {
        for (const auto& s : m_settings) {
            if (s.id() == id) return &s;
        }
        return nullptr;
    }

protected:
    template <typename... Args>
    Setting& addSetting(Args&&... args) {
        return m_settings.emplace_back(std::forward<Args>(args)...);
    }

private:
    std::string m_name;
    std::string m_description;
    Category    m_category;
    int         m_keybind;
    bool        m_enabled = false;
    std::vector<Setting> m_settings;
};

using ModulePtr = std::unique_ptr<Module>;

constexpr const char* categoryName(Category c) {
    switch (c) {
        case Category::Combat:   return "Combat";
        case Category::Movement: return "Movement";
        case Category::Visual:   return "Visual";
        case Category::Player:   return "Player";
        case Category::World:    return "World";
        case Category::Misc:     return "Misc";
    }
    return "Misc";
}

} // namespace glacier
