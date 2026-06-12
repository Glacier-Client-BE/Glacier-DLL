#pragma once

#include "../Module.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Removes darkness by driving the game's gamma to a configurable maximum while
// enabled, restoring the user's original value on disable.
class Fullbright final : public Module {
public:
    Fullbright()
        : Module("Fullbright", "Brightens the world to remove darkness", Category::Visual, 'B') {
        addSetting(Setting{ "gamma", "Gamma", 1.0f, 0.0f, 1.0f, 0.05f });
    }

    // Fullbright works by overriding what Options::getGamma returns (a hook in
    // the SDK). Enable installs the override; disable clears it (-1) so the
    // game's real brightness setting takes over again.
    void onEnable() override {
        apply();
    }

    void onDisable() override {
        sdk::GameSDK::get().setGammaOverride(-1.0f);
    }

    void onTick() override {
        if (enabled()) apply();   // keep the override in sync with the slider
    }

private:
    void apply() {
        if (auto* s = setting("gamma")) {
            sdk::GameSDK::get().setGammaOverride(s->asFloat());
        }
    }
};

} // namespace glacier
