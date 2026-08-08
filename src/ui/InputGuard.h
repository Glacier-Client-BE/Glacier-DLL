#pragma once

#include <utility>

// Freezes player movement/attack/look input while Glacier's menu is open,
// the same way a real Bedrock Screen does (Latite/Flarial included) — the
// player cannot walk, jump, sneak, sprint, attack/use, or turn the camera
// behind the menu.
//
// Why this exists alongside GameSDK::applyCursorState: releaseCursor() only
// controls the game's OWN cursor-grab flag, and in practice that flag
// flipping does not reliably stop either movement or camera look — see the
// history in docs/HANDOFF.md. A Screen normally intercepts all of this
// before gameplay ever sees it; Glacier's menu is a D2D overlay, not a real
// Screen, so nothing was ever intercepting it. This is that interception,
// done at the OS input level (WH_KEYBOARD_LL / WH_MOUSE_LL) — the same
// technique NullMovement already uses for WASD.
//
// Mouse movement is blocked too now (it wasn't originally — see the removed
// comment in git history), which means the real OS cursor stops moving
// while the menu is open. That's why this also maintains its own virtual
// cursor position, fed from the same raw deltas before they're dropped:
// ui::Input reads it instead of GetCursorPos while the menu is open (see
// Input::pollMouse), so the menu can still be clicked with the camera look
// it drives fully decoupled from the game's.
namespace glacier::ui {

class InputGuard {
public:
    static InputGuard& get() {
        static InputGuard instance;
        return instance;
    }

    // Installs the low-level hooks on a dedicated pump thread. Call once,
    // during Glacier::start(). Safe to call again if already active.
    void start();

    // Removes the hooks and joins the pump thread. Call once, during
    // Glacier's teardown.
    void stop();

    // Whether movement/attack/look input is currently being suppressed.
    // Driven every frame from the Present hook off ui::Menu::open(), the
    // same way setCursorReleased is. On the false->true edge, seeds the
    // virtual cursor from the real OS cursor position (via GetCursorPos)
    // so it doesn't jump when the menu opens.
    void setActive(bool active);

    // Client-space virtual cursor position, accumulated from raw mouse
    // deltas while active. Meaningless (and unmoving) while inactive — the
    // real GetCursorPos is authoritative then. Not clamped to the screen;
    // the caller clamps against its own known bounds (see Input::pollMouse).
    std::pair<float, float> cursorPos();

private:
    InputGuard() = default;
};

} // namespace glacier::ui
