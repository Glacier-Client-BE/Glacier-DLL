#pragma once
//
// Base class for all features. Modules subscribe to the EventBus directly in
// their constructor. They own a vector<unique_ptr<Setting>>; settings can be
// added via addSetting<T>(...) helpers.
//
#include "Category.h"
#include "Setting.h"
#include "../events/Events.h"
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Glacier {

class Module {
public:
    Module(std::string id, std::string name, std::string description,
           Category cat, int defaultVK = 0);
    virtual ~Module();

    // Identity
    [[nodiscard]] const std::string& id()          const { return id_; }
    [[nodiscard]] const std::string& displayName() const { return name_; }
    [[nodiscard]] const std::string& description() const { return description_; }
    [[nodiscard]] Category           category()    const { return cat_; }

    // State
    [[nodiscard]] bool enabled() const { return enabled_; }
    void setEnabled(bool v);
    void toggle() { setEnabled(!enabled_); }
    [[nodiscard]] int  keybind() const { return keybind_; }
    void setKeybind(int vk) { keybind_ = vk; }
    [[nodiscard]] bool drawInClickGui() const { return drawInClickGui_; }

    // Settings
    template <class T, class... Args> T& addSetting(Args&&... args) {
        auto s = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *s;
        settings_.push_back(std::move(s));
        return ref;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Setting>>& settings() const { return settings_; }

    // Persistence
    void serialize  (nlohmann::json& out) const;
    void deserialize(const nlohmann::json& in);

    // Drawn in the per-module settings panel. Default: render every setting.
    virtual void drawSettings();

    // Lifecycle hooks (no-op by default).
    virtual void onEnable()  {}
    virtual void onDisable() {}

protected:
    // Convenience: dispatch a key toggle via the global keybind.
    void handleKeybind(KeyEvent& e);

    std::string id_, name_, description_;
    Category    cat_;
    bool        enabled_         = false;
    int         keybind_         = 0;
    bool        drawInClickGui_  = true;
    std::vector<std::unique_ptr<Setting>> settings_;
};

} // namespace Glacier
