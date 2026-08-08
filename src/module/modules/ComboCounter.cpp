#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <cstdint>
#include <string>

namespace glacier {

// Counts consecutive melee swings landed within a time window of each other.
//
// This counts ATTACKS, not confirmed hits. GameSDK::recordAttack fires from
// the same read-only hook ReachDisplay uses, on the game's own attack call —
// there is no server acknowledgement of damage behind it, so a swing that
// missed counts the same as one that connected. Calling this "Combo" the way
// a fighting game would, where every count is a landed hit, would overstate
// what it actually measures.
//
// A new swing is detected by the attack's reported age going down across
// ticks rather than up: lastAttackDistance's age climbs steadily between
// swings and drops back near zero the instant a new one lands, so a
// same-or-larger age than last tick means nothing new happened.
class ComboCounter final : public TextHudModule {
public:
    ComboCounter()
        : TextHudModule("Combo Counter", "Counts consecutive attacks landed in quick succession",
                    Category::Combat, 0,
                    0.01f, 0.45f, 0xFFFFFFFF) {
        addSetting(Setting{ "window", "Reset after (seconds)", 1.5f, 0.3f, 5.0f, 0.1f });
    }

    void onTick() override {
        std::uint64_t ageMs = 0;
        const auto distance = sdk::GameSDK::get().lastAttackDistance(&ageMs);
        const std::uint64_t windowMs =
            static_cast<std::uint64_t>(settingFloat("window", 1.5f) * 1000.0f);

        if (!distance) {
            m_combo = 0;
            m_haveLastAge = false;
            return;
        }

        if (m_haveLastAge && ageMs < m_lastAgeMs) {
            // Age dropped: a new swing just happened. Whether it extends the
            // combo or starts a new one depends on how long the PREVIOUS
            // swing had already been sitting there.
            m_combo = (m_lastAgeMs <= windowMs) ? (m_combo + 1) : 1;
        } else if (!m_haveLastAge) {
            m_combo = 1;
        } else if (ageMs > windowMs) {
            m_combo = 0;
        }

        m_lastAgeMs = ageMs;
        m_haveLastAge = true;
    }

    void buildLines(std::vector<Line>& out) override {
        if (m_combo <= 1) return;   // a "combo" of one swing isn't a combo
        push(out, std::to_string(m_combo) + "x combo");
    }

private:
    int m_combo = 0;
    std::uint64_t m_lastAgeMs = 0;
    bool m_haveLastAge = false;
};

GLACIER_MODULE(ComboCounter);

} // namespace glacier
