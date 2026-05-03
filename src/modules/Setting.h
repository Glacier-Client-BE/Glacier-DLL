#pragma once
//
// Settings are owned by a Module. They expose:
//   - a stable internal name (used as JSON key + display label fallback)
//   - a typed value with min/max where applicable
//   - to/from JSON
//   - a draw() that renders an ImGui control matching the type.
//
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <variant>
#include <cstdint>

namespace Glacier {

enum class SettingType { Bool, Float, Int, Enum, Color, Keybind, Text };

class Setting {
public:
    Setting(std::string name, std::string display, std::string description, SettingType t)
        : name_(std::move(name)), display_(std::move(display)),
          description_(std::move(description)), type_(t) {}
    virtual ~Setting() = default;

    [[nodiscard]] const std::string& name()        const { return name_; }
    [[nodiscard]] const std::string& displayName() const { return display_; }
    [[nodiscard]] const std::string& description() const { return description_; }
    [[nodiscard]] SettingType        type()        const { return type_; }

    virtual void        toJson  (nlohmann::json& out) const = 0;
    virtual void        fromJson(const nlohmann::json& in)  = 0;
    virtual void        draw    () = 0;
    virtual std::string format  () const { return ""; }

protected:
    std::string name_, display_, description_;
    SettingType type_;
};

class BoolSetting final : public Setting {
public:
    BoolSetting(std::string n, std::string d, std::string desc, bool def)
        : Setting(std::move(n), std::move(d), std::move(desc), SettingType::Bool), value_(def) {}
    bool& value() { return value_; }
    bool  get() const { return value_; }
    void  set(bool v) { value_ = v; }
    void toJson(nlohmann::json& out) const override { out[name_] = value_; }
    void fromJson(const nlohmann::json& in) override { if (in.contains(name_)) value_ = in.at(name_).get<bool>(); }
    void draw() override;
private:
    bool value_;
};

class FloatSetting final : public Setting {
public:
    FloatSetting(std::string n, std::string d, std::string desc, float def, float lo, float hi, float step = 0.01f)
        : Setting(std::move(n), std::move(d), std::move(desc), SettingType::Float),
          value_(def), min_(lo), max_(hi), step_(step) {}
    float& value() { return value_; }
    float  get() const { return value_; }
    void   set(float v) { value_ = v; }
    void toJson(nlohmann::json& out) const override { out[name_] = value_; }
    void fromJson(const nlohmann::json& in) override { if (in.contains(name_)) value_ = in.at(name_).get<float>(); }
    void draw() override;
    std::string format() const override;
    float min() const { return min_; } float max() const { return max_; }
private:
    float value_, min_, max_, step_;
};

class IntSetting final : public Setting {
public:
    IntSetting(std::string n, std::string d, std::string desc, int def, int lo, int hi)
        : Setting(std::move(n), std::move(d), std::move(desc), SettingType::Int),
          value_(def), min_(lo), max_(hi) {}
    int& value() { return value_; }
    int  get() const { return value_; }
    void set(int v) { value_ = v; }
    void toJson(nlohmann::json& out) const override { out[name_] = value_; }
    void fromJson(const nlohmann::json& in) override { if (in.contains(name_)) value_ = in.at(name_).get<int>(); }
    void draw() override;
    std::string format() const override;
    int min() const { return min_; } int max() const { return max_; }
private:
    int value_, min_, max_;
};

class EnumSetting final : public Setting {
public:
    EnumSetting(std::string n, std::string d, std::string desc, std::vector<std::string> opts, int def = 0)
        : Setting(std::move(n), std::move(d), std::move(desc), SettingType::Enum),
          options_(std::move(opts)), value_(def) {}
    int  index() const { return value_; }
    void setIndex(int v) { value_ = v; }
    const std::string& current() const { return options_[value_]; }
    const std::vector<std::string>& options() const { return options_; }
    void toJson(nlohmann::json& out) const override { out[name_] = value_; }
    void fromJson(const nlohmann::json& in) override { if (in.contains(name_)) value_ = in.at(name_).get<int>(); }
    void draw() override;
private:
    std::vector<std::string> options_;
    int value_;
};

class ColorSetting final : public Setting {
public:
    ColorSetting(std::string n, std::string d, std::string desc, std::uint32_t defARGB)
        : Setting(std::move(n), std::move(d), std::move(desc), SettingType::Color), value_(defARGB) {}
    std::uint32_t& value() { return value_; }
    std::uint32_t  get() const { return value_; }
    void           set(std::uint32_t v) { value_ = v; }
    void toJson(nlohmann::json& out) const override { out[name_] = value_; }
    void fromJson(const nlohmann::json& in) override { if (in.contains(name_)) value_ = in.at(name_).get<std::uint32_t>(); }
    void draw() override;
private:
    std::uint32_t value_;
};

class KeybindSetting final : public Setting {
public:
    KeybindSetting(std::string n, std::string d, std::string desc, int defaultVK = 0)
        : Setting(std::move(n), std::move(d), std::move(desc), SettingType::Keybind), value_(defaultVK) {}
    int  vkey() const { return value_; }
    void setVKey(int v) { value_ = v; }
    void toJson(nlohmann::json& out) const override { out[name_] = value_; }
    void fromJson(const nlohmann::json& in) override { if (in.contains(name_)) value_ = in.at(name_).get<int>(); }
    void draw() override;
private:
    int value_;
};

class TextSetting final : public Setting {
public:
    TextSetting(std::string n, std::string d, std::string desc, std::string def)
        : Setting(std::move(n), std::move(d), std::move(desc), SettingType::Text), value_(std::move(def)) {}
    std::string& value() { return value_; }
    const std::string& get() const { return value_; }
    void toJson(nlohmann::json& out) const override { out[name_] = value_; }
    void fromJson(const nlohmann::json& in) override { if (in.contains(name_)) value_ = in.at(name_).get<std::string>(); }
    void draw() override;
private:
    std::string value_;
};

} // namespace Glacier
