#include "ModuleSettings.h"
#include <stdexcept>

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

bool ModuleSettings::getBool(const std::string& key) const {
    auto it = m_defs.find(key);
    if (it == m_defs.end()) return false;
    return std::get<bool>(it->second.value);
}

int ModuleSettings::getInt(const std::string& key) const {
    auto it = m_defs.find(key);
    if (it == m_defs.end()) return 0;
    return std::get<int>(it->second.value);
}

float ModuleSettings::getFloat(const std::string& key) const {
    auto it = m_defs.find(key);
    if (it == m_defs.end()) return 0.f;
    return std::get<float>(it->second.value);
}

void ModuleSettings::set(const std::string& key, SettingValue val) {
    auto it = m_defs.find(key);
    if (it != m_defs.end())
        it->second.value = std::move(val);
}
