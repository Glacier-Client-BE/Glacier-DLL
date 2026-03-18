#include "ModuleSettings.h"

void ModuleSettings::defineBool(const std::string& key, const std::string& label, bool def) {
    m_defs[key] = { label, def, false, true };
}

void ModuleSettings::defineInt(const std::string& key, const std::string& label,
                                int def, int min, int max) {
    m_defs[key] = { label, def, min, max };
}

void ModuleSettings::defineFloat(const std::string& key, const std::string& label,
                                  float def, float min, float max) {
    m_defs[key] = { label, def, min, max };
}

void ModuleSettings::defineString(const std::string& key, const std::string& label,
                                   const std::string& def) {
    m_defs[key] = { label, def, std::string{}, std::string{} };
}

bool ModuleSettings::getBool(const std::string& key) const {
    auto it = m_defs.find(key);
    if (it == m_defs.end()) return false;
    const auto* p = std::get_if<bool>(&it->second.value);
    return p ? *p : false;
}

int ModuleSettings::getInt(const std::string& key) const {
    auto it = m_defs.find(key);
    if (it == m_defs.end()) return 0;
    const auto* p = std::get_if<int>(&it->second.value);
    return p ? *p : 0;
}

float ModuleSettings::getFloat(const std::string& key) const {
    auto it = m_defs.find(key);
    if (it == m_defs.end()) return 0.f;
    const auto* p = std::get_if<float>(&it->second.value);
    return p ? *p : 0.f;
}

std::string ModuleSettings::getString(const std::string& key) const {
    auto it = m_defs.find(key);
    if (it == m_defs.end()) return {};
    const auto* p = std::get_if<std::string>(&it->second.value);
    return p ? *p : std::string{};
}

void ModuleSettings::set(const std::string& key, SettingValue val) {
    auto it = m_defs.find(key);
    if (it != m_defs.end())
        it->second.value = std::move(val);
}
