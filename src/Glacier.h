#pragma once

#include <atomic>
#include <Windows.h>

// Top-level client object. Owns startup ordering and the matching reverse
// teardown, plus the WndProc hook that dispatches module keybinds.
//
// Phase 1 has no menu: keybinds are the entire interface. The menu-open key and
// the input-swallowing logic that goes with it arrive with the native UI in
// Phase 2, which is why wndProc here only routes keys to ModuleManager.
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

private:
    Glacier() = default;

    void installWndProc(HWND hwnd);
    void removeWndProc();
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

    HMODULE           m_self        = nullptr;
    WNDPROC           m_origWndProc = nullptr;
    HWND              m_window      = nullptr;
    int               m_unloadKey   = VK_END;
    std::atomic<bool> m_shuttingDown = false;
};

} // namespace glacier
