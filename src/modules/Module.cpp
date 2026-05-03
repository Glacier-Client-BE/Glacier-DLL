#include "Module.h"
#include "../events/EventBus.h"
#include "../core/Logger.h"
#include <imgui.h>

namespace Glacier {

Module::Module(std::string id, std::string name, std::string description,
               Category cat, int defaultVK)
    : id_(std::move(id)), name_(std::move(name)), description_(std::move(description)),
      cat_(cat), keybind_(defaultVK) {
    EventBus::get().listen<KeyEvent, &Module::handleKeybind>(this);
}

Module::~Module() {
    EventBus::get().unlistenAll(this);
}

void Module::setEnabled(bool v) {
    if (v == enabled_) return;
    enabled_ = v;
    if (v) onEnable(); else onDisable();
    Logger::get().info("Mod", id_, " ", v ? "enabled" : "disabled");
}

void Module::handleKeybind(KeyEvent& e) {
    if (!e.down || e.consumedByGui) return;
    if (keybind_ != 0 && e.vkey == keybind_) toggle();
}

void Module::serialize(nlohmann::json& out) const {
    out["enabled"] = enabled_;
    out["keybind"] = keybind_;
    auto& sj = out["settings"];
    sj = nlohmann::json::object();
    for (auto& s : settings_) s->toJson(sj);
}

void Module::deserialize(const nlohmann::json& in) {
    if (in.contains("enabled")) {
        bool e = in.at("enabled").get<bool>();
        if (e != enabled_) setEnabled(e);
    }
    if (in.contains("keybind")) keybind_ = in.at("keybind").get<int>();
    if (in.contains("settings") && in.at("settings").is_object()) {
        for (auto& s : settings_) s->fromJson(in.at("settings"));
    }
}

void Module::drawSettings() {
    for (auto& s : settings_) {
        s->draw();
    }
}

} // namespace Glacier
