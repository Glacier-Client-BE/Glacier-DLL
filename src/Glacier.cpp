#include "Glacier.h"

#include "core/Config.h"
#include "core/EventBus.h"
#include "hook/D3DHook.h"
#include "hook/HookManager.h"
#include "module/ModuleManager.h"
#include "sdk/GameSDK.h"
#include "ui/Input.h"
#include "ui/KeyNames.h"
#include "ui/Menu.h"
#include "ui/Renderer.h"
#include "util/FrameStats.h"
#include "util/Logger.h"

#include <windowsx.h>

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

    sdk::GameSDK::get().installHooks();

    // Restore saved settings. Deliberately after installHooks(): loading can
    // re-enable a module, whose onEnable() may depend on a hook being live, and
    // a module that reports "my hook didn't resolve" should do so accurately.
    Config::get().load();

    // 4. Renderer + present wiring. The overlay owns a private D3D device, so
    //    it is created before the first frame arrives rather than lazily.
    auto& renderer = ui::Renderer::get();
    if (!renderer.initialize()) {
        LOG_WARN("overlay renderer unavailable — client runs headless (keybinds only)");
    }

    auto& d3d = D3DHook::get();
    d3d.onPresent([](IDXGISwapChain* sc, ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        // Sampled before the early-out so the frame rate stays accurate even
        // when the overlay itself can't draw.
        FrameStats::get().onFrame();

        auto& gfx = ui::Renderer::get();
        if (!gfx.beginFrame(sc, dev, ctx)) return;

        RenderEvent ev{ gfx.width(), gfx.height() };
        EventBus::get().publish(ev);

        ModuleManager::get().renderAll();
        ui::Menu::get().render();

        gfx.endFrame();
    });
    d3d.onResize([](IDXGISwapChain*, UINT w, UINT h) {
        ui::Renderer::get().resize(w, h);
    });

    // Not fatal: without the render hook the client still ticks and keybinds
    // still work; only per-frame drawing is lost.
    if (!d3d.initialize()) {
        LOG_WARN("D3D hook init failed — modules will tick but nothing can draw");
    }

    // 5. Capture window input for the menu and module keybinds.
    if (HWND hwnd = d3d.window()) {
        installWndProc(hwnd);
    } else {
        LOG_WARN("no game window — menu and keybinds unavailable");
    }

    // Name the actual keys rather than hardcoding them into the string: they
    // are configurable, and a wrong hint is worse than none when the whole
    // problem is "which key opens this".
    LOG_INFO("Glacier ready — {} or {} opens the menu, {} unloads (change these "
             "under [Glacier] in {})",
             ui::keyDisplayName(menuKey()), ui::keyDisplayName(menuKeyAlt()),
             ui::keyDisplayName(m_unloadKey), Config::path());

    // 6. Logic loop: module ticks + unload watch.
    bool warnedNoFrames = false;
    int  elapsedMs = 0;

    while (!m_shuttingDown.load()) {
        if (GetAsyncKeyState(m_unloadKey) & 0x8000) {
            requestShutdown();
            break;
        }

        ModuleManager::get().tickAll();
        EventBus::get().publish<TickEvent>();

        // Deferred config writes land here, off the render and window threads.
        Config::get().flush();

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

    // Teardown, strict reverse order: stop receiving input, close the menu (so
    // the cursor is handed back), stop receiving frames, remove every hook, then
    // destroy the renderer and the modules those hooks could have called into.
    removeWndProc();
    ui::Menu::get().setOpen(false);

    // Final save before anything is torn down, so state changed since the last
    // menu close isn't lost on unload.
    Config::get().save();

    setCursorReleased(false);
    D3DHook::get().shutdown();
    HookManager::get().shutdown();
    ui::Renderer::get().shutdown();
    ModuleManager::get().shutdown();
    EventBus::get().clear();

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

void Glacier::setCursorReleased(bool released) {
    if (released) {
        ClipCursor(nullptr);
        while (ShowCursor(TRUE) < 0) {}
    } else {
        while (ShowCursor(FALSE) >= 0) {}
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

LRESULT CALLBACK Glacier::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& self = Glacier::get();
    auto& menu = ui::Menu::get();
    auto& in   = ui::Input::get();

    // Bit 30 of lParam is the "was already down" flag — ignoring it means a held
    // key would toggle the module every repeat.
    const bool freshKey = (msg == WM_KEYDOWN) && !(lParam & (1 << 30));

    if (freshKey) {
        const int vk = static_cast<int>(wParam);

        // Menu key first, and never while a keybind widget is capturing —
        // otherwise the menu key can't be bound to anything.
        if (self.isMenuKey(vk) && !menu.capturingKey()) {
            menu.toggle();
            setCursorReleased(menu.open());
            return 0;
        }

        if (menu.open()) {
            // Feed the menu (keybind capture) instead of toggling modules, so
            // rebinding never fires the module being rebound.
            in.onKeyDown(vk);
        } else {
            ModuleManager::get().handleKey(vk);
        }
    }

    if (menu.open()) {
        switch (msg) {
            case WM_MOUSEMOVE:
                in.onMouseMove(static_cast<float>(GET_X_LPARAM(lParam)),
                               static_cast<float>(GET_Y_LPARAM(lParam)));
                break;
            case WM_LBUTTONDOWN: in.onMouseDown(false); break;
            case WM_LBUTTONUP:   in.onMouseUp(false);   break;
            case WM_RBUTTONDOWN: in.onMouseDown(true);  break;
            case WM_RBUTTONUP:   in.onMouseUp(true);    break;
            case WM_MOUSEWHEEL:
                in.onScroll(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
                break;
            case WM_SETCURSOR:
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
            default:
                break;
        }

        // The game is "paused" behind the menu: every remaining game-input
        // message is swallowed so the player can't move, look, or attack while
        // clicking around the UI.
        if (isGameInputMessage(msg)) {
            return 0;
        }
    } else if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) {
        ModuleManager::get().handleClick(msg == WM_RBUTTONDOWN);
    }

    return CallWindowProcW(self.m_origWndProc, hwnd, msg, wParam, lParam);
}

} // namespace glacier
