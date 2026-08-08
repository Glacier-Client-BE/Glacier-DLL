#pragma once

#include "InputGuard.h"
#include "Renderer.h"

#include <algorithm>
#include <Windows.h>

// Input state for the menu and the HUD editor.
//
// The menu is immediate-mode, so it needs *edges* ("was clicked this frame"),
// not just levels ("is down"). The edges are set once per frame and consumed by
// newFrame() at the end of the menu's frame, so a click is never seen twice and
// never missed.
//
// Mouse state is POLLED, not taken from WM_* messages. Bedrock reads the mouse
// through RawInput, so WM_MOUSEMOVE / WM_LBUTTONDOWN are not reliably delivered
// to our WndProc — the menu used to see the cursor parked at (0,0) with no
// clicks at all, which is why nothing in it responded. pollMouse() is called
// once per frame from the Present hook while the menu is open. Position comes
// from InputGuard's virtual cursor rather than GetCursorPos — see
// InputGuard.h and the comment on pollMouse — because InputGuard also blocks
// raw mouse movement from reaching the game (so camera look actually stops),
// which freezes the real OS cursor too. Buttons still come from
// GetAsyncKeyState, which reflects physical state regardless of InputGuard
// blocking the corresponding WM_* messages from propagating further.
//
// pollMouse is the SINGLE owner of button edges. Do not also raise edges from
// the WndProc: the poll observes the physical button before the message
// arrives, so one press would produce two clicks — the same trap that made the
// menu key appear to toggle twice. WndProc contributes only the wheel, which
// cannot be polled.
namespace glacier::ui {

class Input {
public:
    static Input& get() {
        static Input instance;
        return instance;
    }

    // ── Written once per frame by the Present hook ──

    // Samples the cursor position and button levels directly from the OS, and
    // derives this frame's press/release edges from the change since the last
    // poll. See the class comment for why this doesn't come from WM_* messages.
    void pollMouse(HWND hwnd) {
        // InputGuard blocks raw mouse movement from reaching the game (and
        // the real OS cursor) while the menu is open — see InputGuard.h —
        // so GetCursorPos would just read a frozen, stale position here.
        // Its own virtual cursor, accumulated from the same raw deltas
        // before they were dropped, is the live one while the menu is open.
        const auto [vx, vy] = InputGuard::get().cursorPos();
        const auto& renderer = Renderer::get();
        const float maxX = renderer.width()  > 0.0f ? renderer.width()  - 1.0f : 0.0f;
        const float maxY = renderer.height() > 0.0f ? renderer.height() - 1.0f : 0.0f;
        m_x = std::clamp(vx, 0.0f, maxX);
        m_y = std::clamp(vy, 0.0f, maxY);
        (void)hwnd;   // no longer used — kept in the signature, see the .cpp caller

        const bool l = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool r = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        // The first poll after a reset only establishes a baseline. Without
        // this, opening the menu while a button happens to be held would fire a
        // click on whatever the menu drew under the cursor.
        if (!m_polled) {
            m_polled = true;
            m_leftDown = l;
            m_rightDown = r;
            return;
        }

        if (l != m_leftDown)  { l ? onMouseDown(false) : onMouseUp(false); }
        if (r != m_rightDown) { r ? onMouseDown(true)  : onMouseUp(true);  }
    }

    // ── Written by the WndProc hook ──
    void onMouseMove(float x, float y) { m_x = x; m_y = y; }

    void onMouseDown(bool right) {
        if (right) { m_rightDown = true;  m_rightPressed = true; }
        else       { m_leftDown  = true;  m_leftPressed  = true; }
    }

    void onMouseUp(bool right) {
        if (right) { m_rightDown = false; m_rightReleased = true; }
        else       { m_leftDown  = false; m_leftReleased  = true; }
    }

    void onScroll(float delta) { m_scroll += delta; }

    void onKeyDown(int vk) { m_lastKey = vk; }

    // ── Read by the menu ──
    float mouseX() const { return m_x; }
    float mouseY() const { return m_y; }
    bool  leftDown() const { return m_leftDown; }
    bool  leftPressed() const { return m_leftPressed; }
    bool  leftReleased() const { return m_leftReleased; }
    bool  rightPressed() const { return m_rightPressed; }
    float scroll() const { return m_scroll; }

    // Consumes the last key press (returns 0 if none). Used by the keybind
    // capture widget, which must claim the key before anything else sees it.
    int takeKey() {
        const int k = m_lastKey;
        m_lastKey = 0;
        return k;
    }

    // Clears per-frame edge state. Called once at the top of each menu frame.
    void newFrame() {
        m_leftPressed = m_leftReleased = false;
        m_rightPressed = m_rightReleased = false;
        m_scroll = 0.0f;
    }

    void reset() {
        newFrame();
        m_leftDown = m_rightDown = false;
        m_lastKey = 0;
        m_polled = false;
    }

private:
    Input() = default;

    float m_x = 0, m_y = 0;
    bool  m_leftDown = false,  m_leftPressed = false,  m_leftReleased = false;
    bool  m_rightDown = false, m_rightPressed = false, m_rightReleased = false;
    float m_scroll = 0.0f;
    int   m_lastKey = 0;
    bool  m_polled = false;   // has pollMouse established a button baseline?
};

} // namespace glacier::ui
