#include "Glacier.h"

#include "hook/HookManager.h"
#include "hook/D3DHook.h"
#include "module/ModuleManager.h"
#include "sdk/GameSDK.h"
#include "ui/UIRenderer.h"
#include "util/Logger.h"

namespace glacier {

void Glacier::start(HMODULE self) {
    m_self = self;

    Logger::get().attachConsole();
    LOG_INFO("Glacier attaching (build " __DATE__ " " __TIME__ ")");

    // 1. Resolve the game SDK from signatures. Refuse to continue if the core
    //    pointers are missing — better a clean no-op than a crash.
    if (!sdk::GameSDK::get().resolve()) {
        LOG_ERROR("SDK resolution failed — aborting attach");
        return;
    }

    // 2. Register modules.
    ModuleManager::get().initialize();

    // 3. Install hooks.
    if (!HookManager::get().initialize()) {
        LOG_ERROR("hook engine init failed");
        return;
    }

    // SDK's own hooks (ClientInstance capture + gamma override) — needs the
    // hook engine live, so it runs here rather than inside resolve().
    sdk::GameSDK::get().installHooks();

    auto& d3d = D3DHook::get();
    d3d.onPresent([](IDXGISwapChain* sc, ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        ModuleManager::get().renderAll();
        UIRenderer::get().render(sc, dev, ctx);
    });
    d3d.onResize([](IDXGISwapChain*, UINT w, UINT h) {
        UIRenderer::get().onResize(w, h);
    });

    if (!d3d.initialize()) {
        LOG_ERROR("D3D hook init failed");
        return;
    }

    // 4. Capture window input.
    installWndProc(d3d.window());

    LOG_INFO("Glacier ready — INSERT to toggle menu, END to unload");

    // 5. Lightweight logic loop on this thread (module ticks + unload watch).
    while (!m_shuttingDown.load()) {
        if (GetAsyncKeyState(m_unloadKey) & 0x8000) {
            requestShutdown();
            break;
        }
        ModuleManager::get().tickAll();
        Sleep(10);
    }

    // Teardown (reverse order).
    removeWndProc();
    D3DHook::get().shutdown();
    UIRenderer::get().shutdown();
    HookManager::get().shutdown();
    ModuleManager::get().shutdown();

    LOG_INFO("Glacier detached");
    Logger::get().detachConsole();

    FreeLibraryAndExitThread(m_self, 0);
}

void Glacier::requestShutdown() {
    m_shuttingDown.store(true);
}

void Glacier::installWndProc(HWND hwnd) {
    m_window = hwnd;
    m_origWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Glacier::wndProc)));
    LOG_INFO("WndProc hooked");
}

void Glacier::removeWndProc() {
    if (m_window && m_origWndProc) {
        SetWindowLongPtrW(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_origWndProc));
        m_origWndProc = nullptr;
    }
}

bool Glacier::isGameInputMessage(UINT msg) {
    switch (msg) {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
        case WM_KEYDOWN:     case WM_KEYUP:
        case WM_SYSKEYDOWN:  case WM_SYSKEYUP:
        case WM_CHAR:        case WM_INPUT:
            return true;
        default:
            return false;
    }
}

void Glacier::setGamePaused(bool paused) {
    if (paused) {
        // Reveal and free the cursor so the menu is usable.
        ClipCursor(nullptr);
        while (ShowCursor(TRUE) < 0) {}
    } else {
        // Re-hide for gameplay (the game itself keeps the cursor captured).
        while (ShowCursor(FALSE) >= 0) {}
    }
}

LRESULT CALLBACK Glacier::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& self = Glacier::get();
    auto& ui   = UIRenderer::get();

    if (msg == WM_KEYDOWN && !(lParam & (1 << 30) /* ignore key repeat */)) {
        const int vk = static_cast<int>(wParam);
        if (vk == self.menuKey()) {
            const bool open = !ui.menuOpen();
            ui.setMenuOpen(open);
            self.setGamePaused(open);   // freeze + show cursor
            return 0;
        }
        // Keybinds only fire when the menu is closed (so users can rebind in-UI).
        if (!ui.menuOpen()) {
            ModuleManager::get().handleKey(vk);
        }
    }

    // Feed in-game mouse presses to modules (CPS counter, auto-clicker, …).
    // Skipped while the menu is open so UI clicks aren't counted as game clicks.
    if (!ui.menuOpen() && (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN)) {
        ModuleManager::get().handleClick(msg == WM_RBUTTONDOWN);
    }

    // While the menu is open the game is "paused": the overlay consumes the
    // input it wants, and every remaining game-input message is swallowed so the
    // player can't move, look, or attack behind the menu.
    if (ui.menuOpen()) {
        ui.onWndProc(hwnd, msg, wParam, lParam);
        if (msg == WM_SETCURSOR) {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
        }
        if (isGameInputMessage(msg)) {
            return 0;
        }
    }

    return CallWindowProcW(self.m_origWndProc, hwnd, msg, wParam, lParam);
}

} // namespace glacier
