#pragma once

#include <string>
#include <variant>

// A single configurable value attached to a module. The variant keeps storage
// compact while letting the menu render each setting generically from its type
// tag — the widget builder switches on SettingType and needs no per-setting
// glue code, which is what keeps adding a module a one-file change.
namespace glacier {

enum class SettingType { Bool, Float, Int, Key };

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

    // Keybind. Distinguished from Int by type rather than by value range so the
    // menu can render a "press a key" capture widget instead of a slider.
    struct KeyTag {};
    Setting(std::string id, std::string label, KeyTag, int vk)
        : m_id(std::move(id)), m_label(std::move(label)),
          m_type(SettingType::Key), m_value(vk) {}

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
    void set(int v)   { m_value = v; }
    void set(float v) {
        // Clamp on write so a dragged slider can't push a value outside the
        // range the module declared it could handle.
        m_value = v < m_min ? m_min : (v > m_max ? m_max : v);
    }

    // Normalized 0..1 position, for slider rendering and hit-testing.
    float normalized() const {
        const float span = m_max - m_min;
        if (span <= 0.0f) return 0.0f;
        const float v = (m_type == SettingType::Int)
                            ? static_cast<float>(asInt())
                            : asFloat();
        return (v - m_min) / span;
    }

    void setNormalized(float t) {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const float raw = m_min + t * (m_max - m_min);
        if (m_type == SettingType::Int) {
            set(static_cast<int>(raw + 0.5f));
        } else {
            // Quantize to the declared step so the displayed value matches what
            // the module actually receives.
            const float stepped = (m_step > 0.0f)
                                      ? m_min + m_step * static_cast<float>(
                                            static_cast<int>((raw - m_min) / m_step + 0.5f))
                                      : raw;
            set(stepped);
        }
    }

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
