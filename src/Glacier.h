#pragma once

#include <atomic>
#include <Windows.h>

// Top-level client object. Owns startup ordering (SDK -> hooks -> UI) and the
// matching teardown, plus the WndProc hook that feeds input to the overlay and
// dispatches module keybinds. Everything else is a singleton it wires together.
namespace glacier {

class Glacier {
public:
    static Glacier& get() {
        static Glacier instance;
        return instance;
    }

    // Runs on the dedicated init thread spawned from DllMain.
    void start(HMODULE self);

    // Triggered by the unload key (default END). Reverses start() and frees the
    // library. Must run off the loader lock.
    void requestShutdown();
    bool shutdownRequested() const { return m_shuttingDown.load(); }

    HMODULE module() const { return m_self; }

    // The overlay toggle key (default INSERT / RSHIFT). Configurable later.
    int menuKey() const { return m_menuKey; }

private:
    Glacier() = default;

    void installWndProc(HWND hwnd);
    void removeWndProc();
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

    // Freezes/unfreezes the game "behind" the menu: shows the OS cursor and
    // unlocks it while paused so the menu is clickable. Actual input blocking
    // happens in wndProc (all game input is swallowed while the menu is open).
    void setGamePaused(bool paused);
    static bool isGameInputMessage(UINT msg);

    HMODULE           m_self     = nullptr;
    WNDPROC           m_origWndProc = nullptr;
    HWND              m_window   = nullptr;
    int               m_menuKey  = VK_INSERT;
    int               m_unloadKey = VK_END;
    std::atomic<bool> m_shuttingDown = false;
};

} // namespace glacier
