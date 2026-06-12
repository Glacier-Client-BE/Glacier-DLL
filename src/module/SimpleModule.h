#pragma once

#include <initializer_list>
#include <string>

#include "Module.h"

namespace glacier {

// A generic, data-driven module: a name/description/category, optional keybind,
// and a list of settings — no bespoke C++ subclass required. Its behaviour is
// realized the same way every Glacier module's is: the relevant game hook reads
// the module's enabled state + settings (exactly how AutoSprint works). This
// lets the full client feature set be registered declaratively in one table
// instead of one near-empty file per feature.
//
// Modules that need per-frame logic or an always-on HUD widget still get a real
// subclass (see Fullbright, the HUD modules, etc.).
class SimpleModule final : public Module {
public:
    SimpleModule(std::string name, std::string description, Category category,
                 std::initializer_list<Setting> settings = {}, int keybind = 0)
        : Module(std::move(name), std::move(description), category, keybind) {
        for (const auto& s : settings) addSetting(s);
    }
};

} // namespace glacier
