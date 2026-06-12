#pragma once

#include <Windows.h>

#include "../Module.h"
#include "../../hook/D3DHook.h"
#include "../../sdk/GameSDK.h"
#include "../../ui/UIRenderer.h"

namespace glacier {

// Snaps the camera in fixed-angle steps toward the cardinal directions:
// Left/Right arrows yaw by the configured angle (45° default = N/NE/E/…).
// Turns are queued to the SDK and applied on the game thread
// via the LocalPlayer::applyTurnDelta trampoline, so the rotation goes through
// the game's own turn path (sensitivity-independent, no mouse injection).
class SnapLook final : public Module {
public:
    SnapLook()
        : Module("Snap Look", "Snap camera to cardinal directions", Category::Movement) {
        addSetting(Setting{ "angle", "Snap angle (°)", 45.0f, 15.0f, 90.0f, 15.0f });
        addSetting(Setting{ "smooth", "Smooth turn", true });
    }

    void onTick() override {
        // Only act while the game window is focused and the menu is closed.
        if (UIRenderer::get().menuOpen()) { m_left = m_right = false; return; }
        if (GetForegroundWindow() != D3DHook::get().window()) return;
        if (!sdk::GameSDK::get().canTurn()) return;

        float angle = 45.0f;
        if (const auto* s = setting("angle")) angle = s->asFloat();

        // With smoothing on, the snap is spread over a few ticks; without it,
        // the full angle is applied in one turn.
        const auto* sm = setting("smooth");
        const bool smooth = sm && sm->asBool();

        edge(VK_LEFT,  m_left,  [&] { startTurn(-angle, smooth); });
        edge(VK_RIGHT, m_right, [&] { startTurn(+angle, smooth); });

        // Drain an in-flight smooth turn: ~25% of the remainder per tick.
        if (m_remainingYaw != 0.0f) {
            float step = m_remainingYaw * 0.25f;
            if (step > -0.5f && step < 0.5f) step = m_remainingYaw;   // last nudge
            sdk::GameSDK::get().queueTurn(step, 0.0f);
            m_remainingYaw -= step;
        }
    }

    void onDisable() override {
        m_remainingYaw = 0.0f;
        m_left = m_right = false;
    }

private:
    template <typename Fn>
    static void edge(int vk, bool& held, Fn&& onPress) {
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (down && !held) onPress();
        held = down;
    }

    void startTurn(float yawDeg, bool smooth) {
        if (smooth) {
            m_remainingYaw += yawDeg;
        } else {
            sdk::GameSDK::get().queueTurn(yawDeg, 0.0f);
        }
    }

    float m_remainingYaw = 0.0f;
    bool m_left = false, m_right = false;
};

} // namespace glacier
