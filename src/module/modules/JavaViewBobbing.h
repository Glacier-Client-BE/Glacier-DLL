#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>
#include <Windows.h>

#include "../Module.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Java-style view bobbing: the hand/camera sways with walk speed and dips with
// vertical velocity, instead of Bedrock's stiff metronome bob. Velocities are
// derived from SDK position deltas each frame and folded into a smoothed
// translation that the BobHurt hook injects into the bob matrix (the same
// matrix-translate technique Flarial uses).
class JavaViewBobbing final : public Module {
public:
    JavaViewBobbing()
        : Module("Java View Bobbing", "Java-style view bobbing", Category::Visual) {
        addSetting(Setting{ "intensity", "Sway intensity", 1.0f, 0.1f, 3.0f, 0.1f });
        addSetting(Setting{ "jumpFactor", "Jump dip factor", 1.0f, 0.0f, 4.0f, 0.1f });
    }

    void onEnable() override {
        m_lastTime = now();
        m_hasLast = false;
        m_phase = m_transX = m_transY = 0.0f;
    }

    void onDisable() override {
        sdk::GameSDK::get().setBobTranslation(0.0f, 0.0f);
    }

    void onRender() override {
        const std::int64_t t = now();
        const float dt = static_cast<float>(t - m_lastTime) / static_cast<float>(frequency());
        m_lastTime = t;
        if (dt <= 0.0f || dt > 0.25f) return;   // ignore hitches / first frame

        auto pos = sdk::GameSDK::get().playerPosition();
        if (!pos) {
            sdk::GameSDK::get().setBobTranslation(0.0f, 0.0f);
            m_hasLast = false;
            return;
        }

        if (!m_hasLast) {
            m_lastPos = *pos;
            m_hasLast = true;
            return;
        }

        const float vx = (pos->x - m_lastPos.x) / dt;
        const float vy = (pos->y - m_lastPos.y) / dt;
        const float vz = (pos->z - m_lastPos.z) / dt;
        m_lastPos = *pos;

        const float intensity  = settingFloat("intensity", 1.0f);
        const float jumpFactor = settingFloat("jumpFactor", 1.0f);

        // Walk-cycle sway: phase advances with horizontal speed (≈1.8 rad per
        // block walked, matching Java's stride frequency at sprint speed).
        const float horiz = std::sqrt(vx * vx + vz * vz);
        m_phase += horiz * dt * 1.8f;
        const float speedNorm = std::min(horiz / 5.6f, 1.0f);   // 5.6 b/s = sprint
        const float targetX = std::sin(m_phase) * 0.022f * speedNorm * intensity;

        // Vertical dip: jumping/falling pulls the hand opposite to motion.
        const float targetY = std::clamp(-vy * 0.018f * jumpFactor, -0.3f, 0.3f);

        // Exponential smoothing keeps the motion fluid at any frame rate.
        const float k = std::min(dt * 10.0f, 1.0f);
        m_transX += (targetX - m_transX) * k;
        m_transY += (targetY - m_transY) * k;

        sdk::GameSDK::get().setBobTranslation(m_transX, m_transY);
    }

private:
    float settingFloat(std::string_view id, float fallback) const {
        const auto* s = setting(id);
        return s ? s->asFloat() : fallback;
    }

    static std::int64_t now() {
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        return li.QuadPart;
    }
    static std::int64_t frequency() {
        static const std::int64_t freq = [] {
            LARGE_INTEGER li;
            QueryPerformanceFrequency(&li);
            return li.QuadPart;
        }();
        return freq;
    }

    std::int64_t m_lastTime = 0;
    sdk::Vec3    m_lastPos{};
    bool         m_hasLast = false;
    float        m_phase = 0.0f;
    float        m_transX = 0.0f;
    float        m_transY = 0.0f;
};

} // namespace glacier
