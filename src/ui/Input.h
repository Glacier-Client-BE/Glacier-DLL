#pragma once

#include <Windows.h>

// Input state shared between the WndProc hook (which writes it, on the window
// thread) and the menu (which reads it, on the render thread).
//
// The menu is immediate-mode, so it needs *edges* ("was clicked this frame"),
// not just levels ("is down"). WndProc sets the edge flags; the menu consumes
// them once per frame via newFrame(), which clears them. That ordering means a
// click is never seen twice and never missed, even though the two sides run on
// different threads at different rates.
namespace glacier::ui {

class Input {
public:
    static Input& get() {
        static Input instance;
        return instance;
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
    }

private:
    Input() = default;

    float m_x = 0, m_y = 0;
    bool  m_leftDown = false,  m_leftPressed = false,  m_leftReleased = false;
    bool  m_rightDown = false, m_rightPressed = false, m_rightReleased = false;
    float m_scroll = 0.0f;
    int   m_lastKey = 0;
};

} // namespace glacier::ui
