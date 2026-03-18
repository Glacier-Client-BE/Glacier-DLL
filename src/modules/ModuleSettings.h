#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using SettingValue = std::variant<bool, int, float, std::string>;

struct SettingDef {
    std::string  label;
    SettingValue value;
    SettingValue minVal;
    SettingValue maxVal;
};

class ModuleSettings {
public:
    void defineBool  (const std::string& key, const std::string& label, bool defaultVal);
    void defineInt   (const std::string& key, const std::string& label, int   defaultVal, int   min, int   max);
    void defineFloat (const std::string& key, const std::string& label, float defaultVal, float min, float max);
    void defineString(const std::string& key, const std::string& label, const std::string& defaultVal);

    bool        getBool  (const std::string& key) const;
    int         getInt   (const std::string& key) const;
    float       getFloat (const std::string& key) const;
    std::string getString(const std::string& key) const;

    void set(const std::string& key, SettingValue val);

    const std::unordered_map<std::string, SettingDef>& getDefs() const { return m_defs; }

private:
    std::unordered_map<std::string, SettingDef> m_defs;
};
