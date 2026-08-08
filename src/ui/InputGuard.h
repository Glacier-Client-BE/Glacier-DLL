#pragma once

// Freezes player movement/attack input while Glacier's menu is open, the same
// way a real Bedrock Screen does (Latite/Flarial included) — the player
// cannot walk, jump, sneak, sprint, or attack/use behind the menu.
//
// Why this exists alongside GameSDK::applyCursorState: releaseCursor() only
// controls whether the game's OWN cursor-grab flag is set, which is the
// mechanism Bedrock's *camera look* reads. Movement keys (WASD/Space/Shift/
// Ctrl) and mouse buttons are handled by a separate path that does not check
// cursorGrabbed() at all — a Screen normally intercepts them before gameplay
// ever sees them, but Glacier's menu is a D2D overlay, not a real Screen, so
// nothing was ever intercepting them. This is that interception, done at the
// OS input level (WH_KEYBOARD_LL / WH_MOUSE_LL) instead — the same technique
// NullMovement already uses for WASD, just gated on the menu being open
// instead of always active, and covering the mouse buttons too.
//
// Deliberately does NOT touch mouse *movement* (WM_MOUSEMOVE / cursor
// position): blocking that at the OS level would also freeze the OS cursor,
// which the menu itself needs free to move so it can be clicked. Camera look
// stays the job of GameSDK::applyCursorState / setCursorReleased.
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

    // Whether movement/attack input is currently being suppressed. Driven
    // every frame from the Present hook off ui::Menu::open(), the same way
    // setCursorReleased is.
    void setActive(bool active);

private:
    InputGuard() = default;
};

} // namespace glacier::ui
