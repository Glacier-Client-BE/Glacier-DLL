#include "../Module.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"
#include "../../util/Logger.h"

// Modules live entirely in a .cpp with no header: nothing outside the module
// system ever names the type (the registry hands back a Module*), and keeping
// the GLACIER_MODULE registration in exactly one translation unit makes
// double-registration structurally impossible.
namespace glacier {

// Removes darkness by overriding what the game's brightness accessor returns.
//
// This is the thinnest possible end-to-end slice through the whole client —
// signature scan → tracked hook → module toggle — which is exactly why it ships
// first: if Fullbright works, the core is proven. It touches no HUD, no event
// bus, and no game object graph beyond the one hooked function.
class Fullbright final : public Module {
public:
    Fullbright()
        : Module("Fullbright", "Brightens the world to remove darkness", Category::Visual, 'B') {
        addSetting(Setting{ "gamma", "Gamma", 1.0f, 0.0f, 1.0f, 0.05f });
    }

    void onEnable() override {
        if (!sdk::GameSDK::get().gammaHookActive()) {
            LOG_WARN("Fullbright enabled, but Options::getGamma never resolved — no effect. "
                     "See docs/reverse-engineering.md");
        }
        apply();
    }

    // -1 hands control back to the player's real brightness setting. The game's
    // saved value was never written to, so there is nothing else to restore.
    void onDisable() override {
        sdk::GameSDK::get().setGammaOverride(-1.0f);
    }

    // Keeps the override in sync while the user drags the slider.
    void onTick() override {
        if (enabled()) apply();
    }

private:
    void apply() {
        if (const auto* s = setting("gamma")) {
            sdk::GameSDK::get().setGammaOverride(s->asFloat());
        }
    }
};

GLACIER_MODULE(Fullbright);

} // namespace glacier
