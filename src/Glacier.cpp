#include "Glacier.h"

#include "core/Config.h"
#include "core/EventBus.h"
#include "hook/D3DHook.h"
#include "hook/HookManager.h"
#include "memory/GameVersion.h"
#include "module/ModuleManager.h"
#include "sdk/GameSDK.h"
#include "sdk/HitConfirmation.h"
#include "sdk/ItemRendering.h"
#include "ui/Input.h"
#include "ui/KeyNames.h"
#include "ui/Menu.h"
#include "ui/Renderer.h"
#include "util/CrashHandler.h"
#include "util/FrameStats.h"
#include "util/Logger.h"

#include <filesystem>
#include <system_error>
#include <windowsx.h>

namespace glacier {

void Glacier::start(HMODULE self) {
    m_self = self;

    Logger::get().attachConsole();

    // The file log is opened before anything else happens, because everything
    // after this point can take the game down with it — and a console that dies
    // with the process is worthless for exactly that case.
    {
        std::error_code ec;
        std::filesystem::create_directories(Config::directory(), ec);
        Logger::get().openFile(Config::directory() + "\\glacier.log");
    }

    LOG_INFO("Glacier attaching (build " __DATE__ " " __TIME__ ")");
    LOG_INFO("logging to {} (previous session kept as glacier.log.prev)",
             Logger::get().filePath());

    // A crash from here on gets an address and a description instead of just
    // ending the log.
    CrashHandler::install();

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

    // After the config, because whether this installs anything at all is a
    // config setting — see the comment on ItemRendering::setEnabled.
    sdk::ItemRendering::get().installHooks();
    sdk::HitConfirmation::get().installHooks();

    // 4. Renderer + present wiring. The overlay owns a private D3D device, so
    //    it is created before the first frame arrives rather than lazily.
    // The renderer is NOT initialized here. Its device has to be created on the
    // same GPU adapter the game renders on, and that is only knowable once the
    // game hands us its device in the Present hook — so beginFrame creates it
    // on the first frame instead.

    auto& d3d = D3DHook::get();
    d3d.onPresent([](IDXGISwapChain* sc, ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        // Sampled before the early-out so the frame rate stays accurate even
        // when the overlay itself can't draw.
        FrameStats::get().onFrame();

        LOG_ONCE("first Present callback");

        auto& gfx = ui::Renderer::get();
        if (!gfx.beginFrame(sc, dev, ctx)) return;

        LOG_ONCE("first successful beginFrame");

        // Nothing of ours is drawn outside a world — no HUD over the main menu,
        // no menu where there is no player to configure. The frame is still
        // completed rather than returned from early: bailing between beginFrame
        // and endFrame leaves the keyed mutex on the wrong key and deadlocks
        // every frame after it. An empty overlay composites to nothing.
        if (!sdk::GameSDK::get().inGame()) {
            gfx.endFrame();
            return;
        }

        LOG_ONCE("first frame with a live player — the world is loaded");

        // Reconcile the game's cursor grab against the menu, every frame, from
        // the game's own thread — the way Latite's ScreenManager::onUpdate
        // does. The repetition is required, not defensive: the game re-grabs
        // the cursor by itself, so releasing once when the menu opens does not
        // hold and the player keeps moving behind the menu.
        sdk::GameSDK::get().applyCursorState(ui::Menu::get().open());

        // Re-evaluated every frame, not just on the open/close edge: the
        // failure that flips cursorControlWorking() to false is only ever
        // observed mid-session, inside applyCursorState above, so the
        // ShowCursor fallback needs a chance to engage on whichever frame
        // that happens on — not just the one where the menu was toggled.
        setCursorReleased(ui::Menu::get().open());

        // Sample the mouse before anything hit-tests it. Polled rather than
        // taken from WM_* messages because Bedrock consumes the mouse through
        // RawInput — see the comment on ui::Input. Only while the menu is open:
        // the HUD editor is active exactly then, and outside that a poll would
        // feed gameplay clicks into UI state nothing is drawing.
        if (ui::Menu::get().open()) {
            if (HWND hwnd = D3DHook::get().window()) {
                ui::Input::get().pollMouse(hwnd);
            }
        }

        RenderEvent ev{ gfx.width(), gfx.height() };
        EventBus::get().publish(ev);

        LOG_ONCE("first cursor reconcile survived");

        // Modules draw only where Latite and Flarial draw theirs: during actual
        // gameplay, or while Glacier's own menu is open for editing. Gating on
        // inGame() alone left modules on screen over the pause menu, inventory,
        // chat and the loading screen too, because the local player object is
        // still alive through all of those — inGame() cannot tell them apart.
        //
        // cursorGrabbed() can: the game releases its own cursor grab for every
        // one of those screens and re-grabs it the moment gameplay resumes,
        // which is exactly the signal Latite's HUDEditor::renderModules uses
        // (`isActive() || minecraftGame->isCursorGrabbed()`). Glacier's menu
        // releases the cursor too while open, so editing needs its own
        // condition rather than falling out of cursorGrabbed().
        const bool editingHud = ui::Menu::get().open();
        const bool gameplayHud = sdk::GameSDK::get().cursorGrabbed() && !Glacier::get().hudHidden();
        if (editingHud || gameplayHud) {
            ModuleManager::get().renderAll();
        }
        LOG_ONCE("first module render pass survived");

        ui::Menu::get().render();

        // Hand this frame's item-icon requests to the game's UI pass, which
        // draws them at the start of the next frame. See sdk/ItemRendering.h.
        sdk::ItemRendering::get().publish();

        gfx.endFrame();
    });
    d3d.onResize([](IDXGISwapChain*, UINT w, UINT h) {
        ui::Renderer::get().resize(w, h);
    });
    // Registered before initialize(): the first frame can arrive during it, and
    // this fires exactly once. attachToWindow is idempotent, so it does not
    // matter whether this or the provisional attach below runs first.
    // Stores only — the attach happens on the logic loop below. See
    // requestWindowAttach for why doing it here deadlocks the game.
    d3d.onWindowResolved([](HWND hwnd) { Glacier::get().requestWindowAttach(hwnd); });

    // Not fatal: without the render hook the client still ticks and keybinds
    // still work; only per-frame drawing is lost.
    if (!d3d.initialize()) {
        LOG_WARN("D3D hook init failed — modules will tick but nothing can draw");
    }

    // 5. Capture window input for the menu and module keybinds.
    //
    // Provisional: window() is a guess until the first frame, which is why the
    // callback below re-attaches to the window the swapchain actually presents
    // to. Attaching now anyway means keybinds work even if the D3D hook never
    // produces a frame.
    if (HWND hwnd = d3d.window()) {
        attachToWindow(hwnd);
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
    bool warnedNoFrames  = false;
    int  elapsedMs       = 0;

    while (!m_shuttingDown.load()) {
        if (GetAsyncKeyState(m_unloadKey) & 0x8000) {
            requestShutdown();
            break;
        }

        // The ONLY place the menu is toggled. Polling rather than WM_KEYDOWN
        // because Bedrock routes keyboard input through RawInput, so WM_KEYDOWN
        // is not reliably delivered to our WndProc — and GetAsyncKeyState
        // bypasses the message queue entirely.
        //
        // Do not add a second toggle in the WndProc: the two fire at different
        // times for one physical press and cancel each other out.
        //
        // Open and close are asymmetric on purpose:
        //   - G opens AND closes.
        //   - M only opens — it never closes, so it can't double as an
        //     "accidentally close what I just opened" key.
        //   - Escape only closes — it never opens, matching the universal
        //     "Escape backs out of a menu" convention every player already
        //     has muscle memory for.
        // Opening additionally requires cursorGrabbed(): that's false during
        // chat, the pause screen, and inventory, all of which the player can
        // reach mid-typing a chat message containing a "g" or "m" — and
        // GetAsyncKeyState reports the physical key regardless of which
        // window has focus, so without this gate, typing "good game" in chat
        // popped the menu open mid-sentence.
        {
            // The menu belongs to a play session. Outside one there is nothing
            // to configure against, and leaving it open across a disconnect
            // would strand the released cursor — so leaving a world closes it.
            const bool inGame = sdk::GameSDK::get().inGame();
            if (!inGame && ui::Menu::get().open()) {
                ui::Menu::get().setOpen(false);
                setCursorReleased(false);
            }

            const bool gDown   = (GetAsyncKeyState(m_menuKey)     & 0x8000) != 0;
            const bool mDown   = (GetAsyncKeyState(m_menuKeyAlt)  & 0x8000) != 0;
            const bool escDown = (GetAsyncKeyState(VK_ESCAPE)     & 0x8000) != 0;

            const bool gPressed   = gDown   && !m_menuKeyWasDown.load(std::memory_order_relaxed);
            const bool mPressed   = mDown   && !m_menuKeyAltWasDown.load(std::memory_order_relaxed);
            const bool escPressed = escDown && !m_menuCloseKeyWasDown.load(std::memory_order_relaxed);

            if (inGame && !ui::Menu::get().capturingKey()) {
                const bool open = ui::Menu::get().open();
                const bool gameplayActive = sdk::GameSDK::get().cursorGrabbed();

                if (!open && gameplayActive && (gPressed || mPressed)) {
                    ui::Menu::get().setOpen(true);
                    setCursorReleased(true);
                } else if (open && (gPressed || escPressed)) {
                    ui::Menu::get().setOpen(false);
                    setCursorReleased(false);
                }
            }

            // Tracked even when not in game, so walking back into a world with
            // a key still held doesn't immediately act on it.
            m_menuKeyWasDown.store(gDown, std::memory_order_relaxed);
            m_menuKeyAltWasDown.store(mDown, std::memory_order_relaxed);
            m_menuCloseKeyWasDown.store(escDown, std::memory_order_relaxed);
        }

        // F1: hides HUD modules, polled for the same RawInput reason as the
        // menu key. Not toggled while the menu is open — Flarial's F1Listener
        // applies the same guard, so an F1 press meant to rebind something in
        // the menu never also hides the HUD out from under the user.
        {
            const bool inGame = sdk::GameSDK::get().inGame();
            if (!inGame) {
                // Leaving a world resets it, the same way leaving closes the
                // menu: a hidden HUD that silently carried over into the next
                // world would look like modules had stopped working.
                m_hudHidden.store(false, std::memory_order_relaxed);
            } else {
                const bool hudKeyDown = (GetAsyncKeyState(m_hudToggleKey) & 0x8000) != 0;
                if (hudKeyDown && !m_hudToggleKeyWasDown && !ui::Menu::get().open()) {
                    m_hudHidden.store(!m_hudHidden.load(std::memory_order_relaxed),
                                      std::memory_order_relaxed);
                }
                m_hudToggleKeyWasDown = hudKeyDown;
            }
        }

        // Mouse clicks for modules (CPS Counter). Polled for the same reason
        // the menu key is: Bedrock reads the mouse through RawInput, so
        // WM_LBUTTONDOWN does not reliably reach our WndProc and the counter
        // simply never counted. Only while playing — clicks aimed at the menu
        // are not gameplay clicks, and there is nothing to count on the main
        // menu.
        {
            const bool countable = sdk::GameSDK::get().inGame() && !ui::Menu::get().open();
            const bool l = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool r = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

            if (countable) {
                if (l && !m_leftWasDown)  ModuleManager::get().handleClick(false);
                if (r && !m_rightWasDown) ModuleManager::get().handleClick(true);
            }
            // Tracked unconditionally, so a click that started in the menu
            // isn't counted again the moment the menu closes.
            m_leftWasDown  = l;
            m_rightWasDown = r;
        }

        // Re-attach if the first frame reported a different window than the
        // pre-frame guess. Done here, on our own thread, because the calls it
        // makes block on the game's message pump.
        applyPendingWindowAttach();

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
    restoreWindowTitle();
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

    // Last, so a fault during teardown is still attributed.
    CrashHandler::remove();

    LOG_INFO("Glacier detached");
    Logger::get().closeFile();
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

void Glacier::applyPendingWindowAttach() {
    if (HWND hwnd = m_pendingWindow.exchange(nullptr, std::memory_order_relaxed)) {
        attachToWindow(hwnd);
    }
}

void Glacier::attachToWindow(HWND hwnd) {
    if (!hwnd || (hwnd == m_window && m_origWndProc)) return;

    // Undo whatever we did to the previous window first, or a guess that turned
    // out wrong leaves a hooked WndProc and a renamed title behind on it.
    restoreWindowTitle();
    removeWndProc();

    installWndProc(hwnd);
    brandWindow(hwnd);
}

void Glacier::brandWindow(HWND hwnd) {
    // Keep the original so unloading leaves the window exactly as found — the
    // client is unloadable at runtime, and a title that outlives it would be a
    // visible lie about what's attached.
    wchar_t original[256]{};
    if (GetWindowTextW(hwnd, original, static_cast<int>(std::size(original))) > 0) {
        m_originalTitle = original;
    }

    // Report the build the game actually is, not the one Glacier targets. If
    // those differ, the title is the earliest place a user sees it.
    const auto& v = memory::gameVersion();
    wchar_t title[256]{};
    if (v.valid) {
        swprintf_s(title, L"Glacier Client for Minecraft: Bedrock Edition %d.%d.%d",
                   v.major, v.minor, v.patch);
    } else {
        swprintf_s(title, L"Glacier Client for Minecraft: Bedrock Edition");
    }
    SetWindowTextW(hwnd, title);
}

void Glacier::restoreWindowTitle() {
    if (m_window && !m_originalTitle.empty()) {
        SetWindowTextW(m_window, m_originalTitle.c_str());
        m_originalTitle.clear();
    }
}

void Glacier::removeWndProc() {
    if (m_window && m_origWndProc) {
        SetWindowLongPtrW(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_origWndProc));
        m_origWndProc = nullptr;
    }
}

void Glacier::setCursorReleased(bool released) {
    // The real work is GameSDK::applyCursorState, driven every frame from the
    // Present hook — the game re-grabs the cursor on its own, so this could
    // never have been a one-shot call from here.
    //
    // What is left is the fallback for when the cursor signatures are either
    // missing entirely, or resolved to something that doesn't actually work
    // (cursorControlWorking() goes false the first time applyCursorState
    // observes a releaseCursor() call that didn't change cursorGrabbed()).
    // The second case is only detected mid-session, not at the moment this
    // function is first called — which is why the Present hook calls this
    // every frame the menu is open rather than only on the toggle edge, so
    // the fallback engages within a frame or two of the failure, not only
    // on the next open/close.
    if (sdk::GameSDK::get().cursorControlAvailable() && sdk::GameSDK::get().cursorControlWorking()) {
        // If an earlier session had to fall back, undo it now so the OS cursor
        // counter doesn't drift permanently out of balance.
        if (!released && s_cursorFallback) {
            while (ShowCursor(FALSE) >= 0) {}
            s_cursorFallback = false;
        }
        return;
    }

    // Edge-triggered on s_cursorFallback, not level-triggered on `released`:
    // this is now called every frame the menu is open (see the Present hook
    // above), and ShowCursor's return value is its own running counter —
    // calling ShowCursor(TRUE) again every frame after it's already visible
    // would increment that counter without a matching decrement, and the
    // eventual ShowCursor(FALSE) unwind loop would run far more times than
    // it should to rebalance it.
    if (released) {
        if (!s_cursorFallback) {
            ClipCursor(nullptr);
            while (ShowCursor(TRUE) < 0) {}
            s_cursorFallback = true;
        }
    } else if (s_cursorFallback) {
        while (ShowCursor(FALSE) >= 0) {}
        s_cursorFallback = false;
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

        // The menu key is toggled ONLY by the polling loop on the logic thread.
        //
        // It used to be toggled here as well, with a shared "was down" flag
        // meant to stop the two paths colliding. That flag cannot work:
        // GetAsyncKeyState reports the physical key the instant it goes down,
        // while WM_KEYDOWN arrives later via the message queue. The poll fires
        // first (opening the menu and setting the flag), then this handler runs
        // and toggles again (closing it). One press, two toggles — the menu
        // appears to never open, and the close writes the config, which is
        // exactly the "config saved spam with no menu" symptom.
        //
        // We still swallow the key so G/M never leak through to the game.
        if (self.isMenuKey(vk) && !menu.capturingKey()) {
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
            // Position and button edges are NOT taken from here. They are polled
            // once per frame in the Present hook, because Bedrock consumes the
            // mouse through RawInput and these messages may never arrive. Adding
            // them back would double-fire every click — see ui::Input.
            //
            // The wheel is the exception: there is no polling API for it, and a
            // scroll produces no edge that could collide with the poll.
            case WM_MOUSEWHEEL:
                in.onScroll(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
                break;
            case WM_SETCURSOR:
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
            default:
                break;
        }

        // Supplementary only. What actually pauses the game behind the menu is
        // releasing the game's cursor grab (setCursorReleased) — Bedrock reads
        // RawInput, so a swallowed WM_KEYDOWN stops nothing on its own. This
        // remains because it costs nothing and does help for the messages the
        // game does read from the queue.
        if (isGameInputMessage(msg)) {
            return 0;
        }
    }
    // Module clicks are NOT dispatched from here. They are polled on the logic
    // thread for the same RawInput reason as the menu key, and having both
    // would double-count every click in the CPS counter on any build where the
    // messages do arrive.

    return CallWindowProcW(self.m_origWndProc, hwnd, msg, wParam, lParam);
}

} // namespace glacier
