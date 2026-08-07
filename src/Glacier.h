#pragma once

#include <atomic>
#include <string>
#include <Windows.h>

// Top-level client object. Owns startup ordering and the matching reverse
// teardown, plus the WndProc hook that feeds the menu and dispatches module
// keybinds.
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

    // Menu toggle. Two keys are accepted because there is a bootstrapping
    // problem here: if you cannot open the menu, you cannot rebind the key that
    // opens it. G ("Glacier") and M ("mod menu") are both unbound in vanilla
    // Bedrock, so neither steals a gameplay action, and every keyboard has
    // them — unlike INSERT, which compact and laptop layouts often omit.
    // Both are overridable from the config file.
    int  menuKey() const { return m_menuKey; }
    int  menuKeyAlt() const { return m_menuKeyAlt; }
    void setMenuKey(int vk) { m_menuKey = vk; }
    void setMenuKeyAlt(int vk) { m_menuKeyAlt = vk; }
    bool isMenuKey(int vk) const {
        return vk != 0 && (vk == m_menuKey || vk == m_menuKeyAlt);
    }

    int  unloadKey() const { return m_unloadKey; }
    void setUnloadKey(int vk) { m_unloadKey = vk; }

    // F1: hides HUD modules without opening the menu, the way Latite and
    // Flarial's own F1 handling does. Vanilla Bedrock has no native "hide GUI"
    // state to read back — Flarial's F1Listener is purely client-side, a
    // bool it flips itself — so this is the same: a toggle Glacier owns, not
    // a reflection of anything the game is doing.
    int  hudToggleKey() const { return m_hudToggleKey; }
    void setHudToggleKey(int vk) { m_hudToggleKey = vk; }
    bool hudHidden() const { return m_hudHidden.load(std::memory_order_relaxed); }

private:
    Glacier() = default;

    // Records a window to attach to. Called from the D3D hook, i.e. on the
    // GAME'S RENDER THREAD, so it must do nothing but store the handle.
    //
    // The attach itself cannot happen there. SetWindowText and GetWindowText
    // send WM_SETTEXT/WM_GETTEXT synchronously to the thread that owns the
    // window and block until that thread pumps messages — and the game's main
    // thread cannot pump while it is waiting on the render thread we would be
    // blocking. That deadlocks the game hard: no exception, no crash, just a
    // window that stops responding.
    void requestWindowAttach(HWND hwnd) {
        m_pendingWindow.store(hwnd, std::memory_order_relaxed);
    }

    // Performs a pending attach. Must be called from Glacier's own logic
    // thread, where blocking on the game's message pump costs nothing.
    void applyPendingWindowAttach();

    // Points the WndProc hook and the window branding at `hwnd`, undoing both on
    // whatever window they were on before. Idempotent. Logic thread only.
    void attachToWindow(HWND hwnd);

    void installWndProc(HWND hwnd);
    void removeWndProc();

    // Retitles the game window to name the client and the detected game build,
    // and puts the original back on unload.
    void brandWindow(HWND hwnd);
    void restoreWindowTitle();
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

    // Hands the cursor between the game and the menu by driving the game's own
    // grab state (ClientInstance::grabCursor / releaseCursor). That is also what
    // pauses look and movement: the game stops consuming the mouse once the
    // cursor is released. Falls back to ShowCursor only when the game call
    // isn't available.
    static void setCursorReleased(bool released);

    // True while the ShowCursor fallback above is holding the cursor visible,
    // so the matching decrement happens exactly once.
    static inline bool s_cursorFallback = false;
    static bool isGameInputMessage(UINT msg);

    HMODULE           m_self        = nullptr;
    WNDPROC           m_origWndProc = nullptr;
    HWND              m_window      = nullptr;
    std::wstring      m_originalTitle;

    // Set by the render thread, consumed by the logic thread. See
    // requestWindowAttach for why the handoff exists at all.
    std::atomic<HWND> m_pendingWindow{ nullptr };
    int               m_menuKey     = 'G';
    int               m_menuKeyAlt  = 'M';
    int               m_unloadKey   = VK_END;
    int               m_hudToggleKey = VK_F1;
    std::atomic<bool> m_shuttingDown   = false;
    // Shared edge-detection flag for the menu toggle key. Written by both the
    // WndProc hook (window thread) and the GetAsyncKeyState polling loop (logic
    // thread) so the two paths can't both fire for the same physical key press.
    std::atomic<bool> m_menuKeyWasDown = false;

    // Only ever touched on the logic thread — the same poll loop that owns the
    // menu key edge above — so a plain bool is enough for the "was down" side.
    // The value itself is atomic because the Present callback (render thread)
    // reads it every frame to decide whether to draw HUD modules.
    bool              m_hudToggleKeyWasDown = false;
    std::atomic<bool> m_hudHidden = false;

    // Edge state for the polled mouse buttons that feed Module::onClick. Only
    // ever touched on the logic thread, unlike the menu key flag above.
    bool m_leftWasDown  = false;
    bool m_rightWasDown = false;
};

} // namespace glacier
