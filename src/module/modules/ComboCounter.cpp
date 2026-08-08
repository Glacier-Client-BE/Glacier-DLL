#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"
#include "../../sdk/HitConfirmation.h"

#include <cstdint>
#include <string>

#include <Windows.h>

namespace glacier {

// Counts consecutive melee swings confirmed to have landed, following
// reference/latite's ComboCounter.cpp: a swing starts a short window, and a
// HURT_ANIMATION packet arriving inside it extends the combo rather than a
// swing alone.
//
// One piece of Latite's version isn't ported: they additionally require the
// hurt animation's runtimeID to match the entity they swung at.
// Actor::getRuntimeID() has no discoverable signature, offset, or vtable
// index in the reference sources — see sdk/HitConfirmation.h for the full
// explanation. Without it, this counts "a swing, then any actor's hurt
// animation shortly after" — usually the right one, but an unrelated hurt
// animation nearby within the same window would count too. Everything else
// — the 3-second idle reset and the reset when the player takes damage —
// matches Latite's onTick exactly, both from real fields/signals rather than
// a guess.
class ComboCounter final : public TextHudModule {
public:
    ComboCounter()
        : TextHudModule("Combo Counter", "Counts consecutive confirmed hits",
                    Category::Combat, 0,
                    0.01f, 0.45f, 0xFFFFFFFF) {
        addSetting(Setting{ "window", "Confirm window (ms)", 600.0f, 100.0f, 2000.0f, 50.0f });
    }

    void onTick() override {
        auto& sdk = sdk::GameSDK::get();

        // Matches Latite's onTick: a player who just took damage is not
        // mid-combo, regardless of how recently they swung.
        if (sdk.localPlayerInvulnerableTime() > 8) {
            m_combo = 0;
        }

        const std::uint64_t swingStamp = sdk.lastAttackStamp();
        if (swingStamp == 0) return;   // never swung this session

        const std::uint64_t now = GetTickCount64();

        // Idle timeout, measured from the swing rather than a confirmed hit —
        // Latite's own lastHurt is likewise set at swing time, not on the
        // packet arriving.
        if (now - swingStamp > 3000) {
            m_combo = 0;
            m_lastConsumedSwing = 0;
            return;
        }

        if (swingStamp == m_lastConsumedSwing) return;   // already resolved this swing

        const auto hurtAge = sdk::HitConfirmation::get().lastHurtAnimationAge();
        if (!hurtAge) return;

        const std::uint64_t hurtStamp = now - *hurtAge;
        const auto window = static_cast<std::uint64_t>(settingFloat("window", 600.0f));

        // The hurt animation has to land AT OR AFTER the swing (never before
        // it) and within the confirm window — otherwise it's a stale signal
        // from before this swing happened.
        if (hurtStamp >= swingStamp && (hurtStamp - swingStamp) <= window) {
            ++m_combo;
            m_lastConsumedSwing = swingStamp;
        }
    }

    void buildLines(std::vector<Line>& out) override {
        if (m_combo <= 1) return;   // a "combo" of one confirmed hit isn't a combo
        push(out, std::to_string(m_combo) + "x combo");
    }

private:
    int m_combo = 0;
    std::uint64_t m_lastConsumedSwing = 0;
};

GLACIER_MODULE(ComboCounter);

} // namespace glacier
