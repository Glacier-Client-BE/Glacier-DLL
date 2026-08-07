#pragma once

#include <string>

// Detects which Minecraft: Bedrock build Glacier is actually attached to.
//
// This exists because signatures are version-locked. Glacier's imported table
// targets one specific build; run it against a different one and patterns
// silently stop matching, or — far worse — a pattern still matches but at the
// wrong address, and offsets read plausible-looking garbage. Knowing the real
// version at attach time turns "why is the armor HUD showing nonsense" into a
// one-line log message.
//
// The version comes from the executable's own version resource
// (Minecraft.Windows.exe), which the UWP package stamps with the store build
// number. No signature is involved, so this works even when every pattern in
// the table has gone stale — which is exactly when it is most needed.
namespace glacier::memory {

struct GameVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    int build = 0;
    bool valid = false;

    std::string toString() const;

    // Ordered comparison against a target, ignoring the trailing build number:
    // Mojang bumps it for store repackages that don't move any code.
    bool atLeast(int maj, int min, int pat) const;
    bool sameFeatureRelease(int maj, int min, int pat) const {
        return valid && major == maj && minor == min && patch == pat;
    }
};

// Reads and caches the attached game's version. Safe to call repeatedly.
const GameVersion& gameVersion();

} // namespace glacier::memory
