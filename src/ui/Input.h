#pragma once

#include "Renderer.h"

#include <algorithm>
#include <string>
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
// once per frame from the Present hook while the menu is open: position from
// GetCursorPos, buttons from GetAsyncKeyState.
//
// GetCursorPos is only trustworthy here because ui::InputGuard has actually
// taken the pointer away from the game by then — unclipped, visible, and no
// longer being re-centred every frame. Without that it reads a pointer the
// game has pinned to the middle of the screen, which is exactly what "the
// menu is open but I can't click anything" looked like.
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
        // The real OS pointer, in client coordinates. InputGuard takes the
        // pointer away from the game while the menu is open (unclipping it,
        // making it visible, and stopping the game re-centring it), so it
        // genuinely tracks the mouse here — an earlier design read a virtual
        // cursor accumulated from raw deltas instead, which was only ever
        // needed because the pointer was still frozen by the game's own clip.
        const auto& renderer = Renderer::get();
        const float maxX = renderer.width()  > 0.0f ? renderer.width()  - 1.0f : 0.0f;
        const float maxY = renderer.height() > 0.0f ? renderer.height() - 1.0f : 0.0f;

        POINT p{};
        if (GetCursorPos(&p) && hwnd) {
            ScreenToClient(hwnd, &p);

            // The client area and the back buffer are not always the same
            // size — DPI scaling and borderless-fullscreen transitions both
            // make them differ, and the overlay's coordinates are the back
            // buffer's. Scaling here keeps the cursor on the widget it is
            // visually over.
            RECT client{};
            float sx = 1.0f, sy = 1.0f;
            if (GetClientRect(hwnd, &client)) {
                const float cw = static_cast<float>(client.right - client.left);
                const float ch = static_cast<float>(client.bottom - client.top);
                if (cw > 0.0f && renderer.width()  > 0.0f) sx = renderer.width()  / cw;
                if (ch > 0.0f && renderer.height() > 0.0f) sy = renderer.height() / ch;
            }

            m_x = std::clamp(static_cast<float>(p.x) * sx, 0.0f, maxX);
            m_y = std::clamp(static_cast<float>(p.y) * sy, 0.0f, maxY);
        }

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

    // Typed text, for the menu's search box. Buffered rather than applied
    // directly because this arrives on the window thread while the menu is
    // drawn on the render thread; takeChars() drains it there.
    void onChar(wchar_t c) {
        if (m_charCount < kMaxChars && (c == L'\b' || c >= L' ')) {
            m_chars[m_charCount++] = c;
        }
    }

    // Returns and clears everything typed since the last call.
    std::wstring takeChars() {
        std::wstring out(m_chars, m_chars + m_charCount);
        m_charCount = 0;
        return out;
    }

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
        m_charCount = 0;
        m_polled = false;
    }

private:
    Input() = default;

    static constexpr int kMaxChars = 32;

    float m_x = 0, m_y = 0;
    bool  m_leftDown = false,  m_leftPressed = false,  m_leftReleased = false;
    bool  m_rightDown = false, m_rightPressed = false, m_rightReleased = false;
    float m_scroll = 0.0f;
    int   m_lastKey = 0;
    bool  m_polled = false;   // has pollMouse established a button baseline?

    wchar_t m_chars[kMaxChars]{};
    int     m_charCount = 0;
};

} // namespace glacier::ui
