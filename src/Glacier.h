#pragma once

#include <atomic>
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

private:
    Glacier() = default;

    void installWndProc(HWND hwnd);
    void removeWndProc();
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

    // Frees and reveals the cursor while the menu is open. Input blocking
    // itself happens in wndProc — this only handles cursor visibility, which
    // the game would otherwise keep captured and hidden.
    static void setCursorReleased(bool released);
    static bool isGameInputMessage(UINT msg);

    HMODULE           m_self        = nullptr;
    WNDPROC           m_origWndProc = nullptr;
    HWND              m_window      = nullptr;
    int               m_menuKey     = 'G';
    int               m_menuKeyAlt  = 'M';
    int               m_unloadKey   = VK_END;
    std::atomic<bool> m_shuttingDown = false;
};

} // namespace glacier
