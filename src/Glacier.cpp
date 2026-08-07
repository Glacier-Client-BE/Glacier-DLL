#include "Glacier.h"

#include "hook/D3DHook.h"
#include "hook/HookManager.h"
#include "module/ModuleManager.h"
#include "sdk/GameSDK.h"
#include "util/Logger.h"

namespace glacier {

void Glacier::start(HMODULE self) {
    m_self = self;

    Logger::get().attachConsole();
    LOG_INFO("Glacier attaching (build " __DATE__ " " __TIME__ ")");

    // 1. Resolve the game SDK from signatures. Refuse to continue if the core
    //    pointers are missing — a clean no-op with a named-signature log beats
    //    a crash, and turns a game-update mismatch into a one-file fix.
    if (!sdk::GameSDK::get().resolve()) {
        LOG_ERROR("SDK resolution failed — aborting attach. "
                  "See docs/reverse-engineering.md to re-derive the missing signatures.");
        Logger::get().detachConsole();
        FreeLibraryAndExitThread(m_self, 0);
        return;
    }

    // 2. Construct modules from the self-registering registry.
    ModuleManager::get().initialize();

    // 3. Bring up the hook engine, then everything that installs hooks.
    if (!HookManager::get().initialize()) {
        LOG_ERROR("hook engine init failed");
        ModuleManager::get().shutdown();
        Logger::get().detachConsole();
        FreeLibraryAndExitThread(m_self, 0);
        return;
    }

    // SDK hooks (ClientInstance capture + gamma override) need the engine live,
    // so they install here rather than inside resolve().
    sdk::GameSDK::get().installHooks();

    auto& d3d = D3DHook::get();
    d3d.onPresent([](IDXGISwapChain*, ID3D11Device*, ID3D11DeviceContext*) {
        ModuleManager::get().renderAll();
    });

    // Not fatal: without the render hook the client still ticks and keybinds
    // still work; only per-frame drawing is lost (nothing draws yet in Phase 1).
    if (!d3d.initialize()) {
        LOG_WARN("D3D hook init failed — modules will tick but nothing can draw");
    }

    // 4. Capture window input for module keybinds.
    if (HWND hwnd = d3d.window()) {
        installWndProc(hwnd);
    } else {
        LOG_WARN("no game window — keybinds unavailable");
    }

    LOG_INFO("Glacier ready — B toggles Fullbright, END unloads");

    // 5. Logic loop: module ticks + unload watch.
    bool warnedNoFrames = false;
    int  elapsedMs = 0;

    while (!m_shuttingDown.load()) {
        if (GetAsyncKeyState(m_unloadKey) & 0x8000) {
            requestShutdown();
            break;
        }
        ModuleManager::get().tickAll();
        Sleep(10);
        elapsedMs += 10;

        // A Present hook that never fires is otherwise a silent failure — the
        // client looks healthy and simply does nothing. Say so once.
        if (!warnedNoFrames && elapsedMs > 5000) {
            warnedNoFrames = true;
            if (d3d.framesSeen() == 0) {
                LOG_WARN("no frames observed after 5s — the Present hook is not on the "
                         "swapchain the game renders through");
            }
        }
    }

    // Teardown, strict reverse order: stop receiving input, stop receiving
    // frames, remove every hook, then destroy the modules those hooks could
    // have called into. Getting this order wrong is how a client crashes the
    // game on unload.
    removeWndProc();
    D3DHook::get().shutdown();
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

LRESULT CALLBACK Glacier::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& self = Glacier::get();

    // Bit 30 of lParam is the "was already down" flag — ignoring it means a held
    // key would toggle the module every repeat.
    if (msg == WM_KEYDOWN && !(lParam & (1 << 30))) {
        ModuleManager::get().handleKey(static_cast<int>(wParam));
    }

    if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) {
        ModuleManager::get().handleClick(msg == WM_RBUTTONDOWN);
    }

    return CallWindowProcW(self.m_origWndProc, hwnd, msg, wParam, lParam);
}

} // namespace glacier
