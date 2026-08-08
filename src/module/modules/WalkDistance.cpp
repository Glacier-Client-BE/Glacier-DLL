#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

namespace glacier {

// Running total of horizontal distance covered, in blocks.
//
// Resets when the module is (re-)enabled or the player leaves the world, not
// silently on every world join — a running total that resets itself without
// being asked is the one behaviour a "total" widget must never have.
class WalkDistance final : public TextHudModule {
public:
    WalkDistance()
        : TextHudModule("Walk Distance", "Tracks total distance travelled",
                    Category::Movement, 0,
                    0.01f, 0.37f, 0xFFFFFFFF) {}

    void onEnable() override {
        m_total = 0.0f;
        m_last.reset();
    }

    void onTick() override {
        const auto pos = sdk::GameSDK::get().playerPosition();
        if (!pos) {
            m_last.reset();
            return;
        }

        if (m_last) {
            const float dx = pos->x - m_last->x;
            const float dz = pos->z - m_last->z;
            const float dist = std::sqrt(dx * dx + dz * dz);
            // Same world-load/teleport guard as Speedometer: one huge tick
            // must not get added to a total that is supposed to mean "walked".
            if (dist < 50.0f) m_total += dist;
        }
        m_last = pos;
    }

    void buildLines(std::vector<Line>& out) override {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f blocks", m_total);
        push(out, buf);
    }

private:
    float m_total = 0.0f;
    std::optional<sdk::Vec3> m_last;
};

GLACIER_MODULE(WalkDistance);

} // namespace glacier
