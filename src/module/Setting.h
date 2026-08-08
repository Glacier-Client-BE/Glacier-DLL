#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// A single configurable value attached to a module. The variant keeps storage
// compact while letting the menu render each setting generically from its type
// tag — the widget builder switches on SettingType and needs no per-setting
// glue code, which is what keeps adding a module a one-file change.
namespace glacier {

enum class SettingType { Bool, Float, Int, Key, Color, Enum };

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

    // A choice from a fixed list, stored as the selected index.
    //
    // Distinct from Int for the same reason Key and Color are: an Int renders
    // as a slider, and dragging a slider between "Left", "Center" and "Right"
    // is a genuinely bad control for a choice — you cannot see the options,
    // and the label only tells you where you landed after you let go. The
    // menu renders this as a row of chips instead.
    struct EnumTag {};
    Setting(std::string id, std::string label, EnumTag,
            std::vector<std::string> options, int value = 0)
        : m_id(std::move(id)), m_label(std::move(label)),
          m_type(SettingType::Enum), m_value(value),
          m_min(0.0f), m_max(static_cast<float>(options.empty() ? 0 : options.size() - 1)),
          m_step(1.0f), m_options(std::move(options)) {}

    const std::vector<std::string>& options() const { return m_options; }

    // Clamped, so a config file naming an option that no longer exists reads
    // back as the first one rather than indexing off the end.
    int selectedIndex() const {
        const int i = asInt();
        if (m_options.empty()) return 0;
        return i < 0 ? 0 : (i >= static_cast<int>(m_options.size())
                                ? static_cast<int>(m_options.size()) - 1 : i);
    }

    // Color, stored as 0xAARRGGBB in the int slot. Tagged rather than inferred
    // so the menu renders a swatch and channel sliders instead of a number.
    struct ColorTag {};
    Setting(std::string id, std::string label, ColorTag, std::uint32_t argb)
        : m_id(std::move(id)), m_label(std::move(label)),
          m_type(SettingType::Color), m_value(static_cast<int>(argb)) {}

    std::uint32_t asColor() const { return static_cast<std::uint32_t>(std::get<int>(m_value)); }
    void setColor(std::uint32_t argb) { m_value = static_cast<int>(argb); }

    // Channel accessors for the picker widget. 0=A, 1=R, 2=G, 3=B.
    int channel(int index) const {
        const std::uint32_t c = asColor();
        return static_cast<int>((c >> (8 * (3 - index))) & 0xFF);
    }
    void setChannel(int index, int value) {
        value = value < 0 ? 0 : (value > 255 ? 255 : value);
        const int shift = 8 * (3 - index);
        const std::uint32_t cleared = asColor() & ~(0xFFu << shift);
        setColor(cleared | (static_cast<std::uint32_t>(value) << shift));
    }

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
    // Empty for every type but Enum.
    std::vector<std::string> m_options;
};

} // namespace glacier
