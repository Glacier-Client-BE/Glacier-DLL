#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

namespace glacier {

// Horizontal speed, in blocks per second.
//
// There is no velocity accessor in the SDK, so this derives speed from the
// change in playerPosition() between ticks — the same technique WalkDistance
// uses, kept separate rather than shared because the two want different
// things done with a position delta (an instantaneous rate here, a running
// total there), and a shared "delta tracker" abstraction for two five-line
// call sites would be more machinery than the thing it replaces.
class Speedometer final : public TextHudModule {
public:
    Speedometer()
        : TextHudModule("Speedometer", "Shows your horizontal speed",
                    Category::Movement, 0,
                    0.01f, 0.33f, 0xFFFFFFFF) {
        addSetting(Setting{ "vertical", "Include vertical speed", false });
    }

    void onTick() override {
        const auto pos = sdk::GameSDK::get().playerPosition();
        const ULONGLONG now = GetTickCount64();

        if (!pos) {
            m_last.reset();
            m_speed = 0.0f;
            return;
        }

        if (m_last) {
            const float dt = static_cast<float>(now - m_lastTick) / 1000.0f;
            if (dt > 0.0f) {
                float dx = pos->x - m_last->x;
                float dz = pos->z - m_last->z;
                float distSq = dx * dx + dz * dz;
                if (settingBool("vertical", false)) {
                    const float dy = pos->y - m_last->y;
                    distSq += dy * dy;
                }
                // A world-load or teleport produces a huge one-tick jump that
                // would otherwise flash an absurd number for a frame.
                const float dist = std::sqrt(distSq);
                m_speed = (dist < 50.0f) ? dist / dt : m_speed;
            }
        }

        m_last = pos;
        m_lastTick = now;
    }

    void buildLines(std::vector<Line>& out) override {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f b/s", m_speed);
        push(out, buf);
    }

private:
    std::optional<sdk::Vec3> m_last;
    ULONGLONG m_lastTick = 0;
    float m_speed = 0.0f;
};

GLACIER_MODULE(Speedometer);

} // namespace glacier
