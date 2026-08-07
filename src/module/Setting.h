#pragma once

#include <string>
#include <variant>

// A single configurable value attached to a module. The variant keeps storage
// compact while letting the UI bridge serialize each setting generically to
// JSON (type-tagged) without per-setting glue code.
namespace glacier {

enum class SettingType { Bool, Float, Int, Enum, Key };

class Setting {
public:
    Setting(std::string id, std::string label, bool value)
        : m_id(std::move(id)), m_label(std::move(label)),
          m_type(SettingType::Bool), m_value(value) {}

    Setting(std::string id, std::string label, float value, float min, float max, float step = 0.1f)
        : m_id(std::move(id)), m_label(std::move(label)),
          m_type(SettingType::Float), m_value(value), m_min(min), m_max(max), m_step(step) {}

    Setting(std::string id, std::string label, int value, int min, int max)
        : m_id(std::move(id)), m_label(std::move(label)),
          m_type(SettingType::Int), m_value(value),
          m_min(static_cast<float>(min)), m_max(static_cast<float>(max)), m_step(1.0f) {}

    const std::string& id() const { return m_id; }
    const std::string& label() const { return m_label; }
    SettingType type() const { return m_type; }
    float min() const { return m_min; }
    float max() const { return m_max; }
    float step() const { return m_step; }

    bool  asBool()  const { return std::get<bool>(m_value); }
    float asFloat() const { return std::get<float>(m_value); }
    int   asInt()   const { return std::get<int>(m_value); }

    void set(bool v)  { m_value = v; }
    void set(float v) { m_value = v; }
    void set(int v)   { m_value = v; }

private:
    std::string m_id;
    std::string m_label;
    SettingType m_type;
    std::variant<bool, float, int> m_value;
    float m_min  = 0.0f;
    float m_max  = 1.0f;
    float m_step = 0.1f;
};

} // namespace glacier
