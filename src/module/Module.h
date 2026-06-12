#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Setting.h"
#include "../util/Json.h"

// Base class for every feature ("module" / "cheat"). Modules are pure logic:
// they expose settings and react to enable/disable + per-frame ticks. They know
// nothing about the UI — the JsBridge reflects them to the web layer and pushes
// toggles back. This mirrors the module/manager split used by Latite & Flarial.
namespace glacier {

enum class Category { Combat, Movement, Visual, Player, World, Misc };

class Module {
public:
    Module(std::string name, std::string description, Category category, int keybind = 0)
        : m_name(std::move(name)), m_description(std::move(description)),
          m_category(category), m_keybind(keybind) {}

    virtual ~Module() = default;

    // Lifecycle — overridden by concrete modules.
    virtual void onEnable()  {}
    virtual void onDisable() {}
    virtual void onTick()    {}                  // once per game tick
    virtual void onRender()  {}                  // once per present (overlay drawing)
    virtual void onClick(bool /*rightButton*/) {} // mouse press (CPS, auto-clicker…)

    // Always-on HUD contribution. A module that draws an on-screen widget sets
    // hasHud() and appends one serialized object to `huds` in writeHud(). The
    // bridge collects these every HUD tick and the web layer paints them — the
    // bridge never needs to know the concrete module type.
    virtual bool hasHud() const { return false; }
    virtual void writeHud(json::Array& /*huds*/) const {}

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
    int keybind() const { return m_keybind; }
    void setKeybind(int vk) { m_keybind = vk; }

    std::vector<Setting>& settings() { return m_settings; }
    const std::vector<Setting>& settings() const { return m_settings; }

    Setting* setting(std::string_view id) {
        for (auto& s : m_settings) {
            if (s.id() == id) return &s;
        }
        return nullptr;
    }

    // const overload so const methods (HUD serializers, getters) can read
    // settings without mutating.
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
