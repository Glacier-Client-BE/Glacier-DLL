#include "SwapChainHook.h"
#include "HookManager.h"
#include "../render/Renderer.h"
#include "../render/ModMenu.h"
#include "../modules/ModuleManager.h"
#include "../utils/Logger.h"
#include "../utils/ClientConfig.h"
#include "../sdk/ClientInstance.h"
#include "../icons/IconManager.h"
#include <imgui_impl_win32.h>
#include <dxgi.h>
#include <d3d11.h>

using PresentFn       = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

static PresentFn       g_origPresent       = nullptr;
static ResizeBuffersFn g_origResizeBuffers  = nullptr;
static WNDPROC         g_origWndProc        = nullptr;
static HWND            g_hwnd               = nullptr;

// ── Retrieve MCBE version from the window title ───────────────────────────────
static std::string getMCBEVersion() {
    if (!g_hwnd) return "";
    char title[256]{};
    GetWindowTextA(g_hwnd, title, sizeof(title));
    // Title usually contains something like "Minecraft 1.21.x.x"
    std::string t(title);
    auto pos = t.find("1.");
    if (pos != std::string::npos) {
        // Extract up to next space or end
        auto end = t.find(' ', pos);
        return t.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }
    return "26.x";
}

static void updateWindowTitle() {
    if (!g_hwnd) return;
    std::string ver = getMCBEVersion();
    std::string title = "Glacier Client v" GLACIER_VERSION
        + std::string(" | MCBE v") + (ver.empty() ? "26.x" : ver);
    SetWindowTextA(g_hwnd, title.c_str());
}

// ── WndProc ───────────────────────────────────────────────────────────────────
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK hookedWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return TRUE;

    bool menuOpen = ModMenu::get().isOpen();

    // Toggle menu key
    if (msg == WM_KEYDOWN && (int)wp == ClientConfig::get().menuKey) {
        ModMenu::get().toggle();
        return 0;
    }

    // ── GAME PAUSE when menu is open ──────────────────────────────────────
    if (menuOpen) {
        // Block ALL keyboard input reaching the game
        switch (msg) {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
                return 0;
        }
        // Block mouse input (movement + clicks) so the game doesn't rotate/attack
        switch (msg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return 0;
        }
        // Release mouse grab if the game holds exclusive capture
        if (msg == WM_SETCURSOR) {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        }
    }

    return CallWindowProcA(g_origWndProc, hwnd, msg, wp, lp);
}

// ── Raw input capture block (Minecraft uses raw input for camera) ─────────────
// We register our own RIDEV_NOLEGACY to swallow raw mouse when menu is open.
static RAWINPUTDEVICE g_ridBlock = { 0x01, 0x02, RIDEV_NOLEGACY, nullptr };
static RAWINPUTDEVICE g_ridOrig  = { 0x01, 0x02, RIDEV_REMOVE,   nullptr };

static void blockRawMouse(bool block) {
    if (block) RegisterRawInputDevices(&g_ridBlock, 1, sizeof(RAWINPUTDEVICE));
    else       RegisterRawInputDevices(&g_ridOrig,  1, sizeof(RAWINPUTDEVICE));
}

// ── Present hook ──────────────────────────────────────────────────────────────
static HRESULT STDMETHODCALLTYPE hookedPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    static bool s_init = false;

    if (!s_init) {
        ID3D11Device* dev = nullptr;
        sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev));
        if (dev) {
            if (Renderer::get().init(dev, sc)) {
                ModMenu::get().init();

                // Hook WndProc
                DXGI_SWAP_CHAIN_DESC desc{};
                sc->GetDesc(&desc);
                g_hwnd = desc.OutputWindow;
                g_origWndProc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC,
                                      reinterpret_cast<LONG_PTR>(hookedWndProc)));

                // Init icon manager
                IconManager::get().init(dev, ClientConfig::get().dllDirectory);

                // Set custom window title
                updateWindowTitle();

                // Setup raw input block descriptors
                g_ridBlock.hwndTarget = g_hwnd;

                s_init = true;
                Logger::info("SwapChainHook: fully initialized");
            }
            dev->Release();
        }
    }

    if (Renderer::get().isReady()) {
        bool menuOpen = ModMenu::get().isOpen();

        // Pause game ticks when menu is open
        if (!menuOpen) {
            ModuleManager::get().onTick();
            auto* lp = getLocalPlayer();
            TargetTracker::get().update(lp);
        }

        // Toggle raw mouse block
        static bool s_lastMenuOpen = false;
        if (menuOpen != s_lastMenuOpen) {
            blockRawMouse(menuOpen);
            if (menuOpen) {
                // Show system cursor
                ShowCursor(TRUE);
                // Update title each open to catch version changes
                updateWindowTitle();
            } else {
                ShowCursor(FALSE);
            }
            s_lastMenuOpen = menuOpen;
        }

        Renderer::get().beginFrame();

        // In-game HUD rendering (always, even when paused)
        ImDrawList* dl = Renderer::get().getBackgroundDrawList();
        if (dl) ModuleManager::get().onRender(dl);
        ModuleManager::get().onRenderImGui();

        ModMenu::get().render();

        Renderer::get().endFrame();
    }

    return g_origPresent(sc, sync, flags);
}

// ── ResizeBuffers ─────────────────────────────────────────────────────────────
static HRESULT STDMETHODCALLTYPE hookedResizeBuffers(
    IDXGISwapChain* sc, UINT bc, UINT w, UINT h, DXGI_FORMAT fmt, UINT f)
{
    Renderer::get().invalidateRTV();
    return g_origResizeBuffers(sc, bc, w, h, fmt, f);
}

// ── SwapChainHook ─────────────────────────────────────────────────────────────
SwapChainHook& SwapChainHook::get() { static SwapChainHook i; return i; }

bool SwapChainHook::init() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount        = 1;
    sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow       = GetForegroundWindow();
    sd.SampleDesc.Count   = 1;
    sd.Windowed           = TRUE;
    sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device*        pDev = nullptr;
    ID3D11DeviceContext* pCtx = nullptr;
    IDXGISwapChain*      pSC  = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &pSC, &pDev, nullptr, &pCtx);

    if (FAILED(hr)) {
        Logger::error("SwapChainHook: D3D11CreateDeviceAndSwapChain failed: 0x{:08X}",
                      static_cast<uint32_t>(hr));
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(pSC);
    HookManager::get().install(vtable[8],  (void*)hookedPresent,      (void**)&g_origPresent);
    HookManager::get().install(vtable[13], (void*)hookedResizeBuffers, (void**)&g_origResizeBuffers);
    HookManager::get().enableAll();

    pSC->Release(); pDev->Release(); pCtx->Release();

    Logger::info("SwapChainHook: Present + ResizeBuffers hooked");
    return true;
}

void SwapChainHook::shutdown() {
    IconManager::get().shutdown();
    if (g_origWndProc && g_hwnd)
        SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
    g_origWndProc = nullptr;
    blockRawMouse(false);
    Logger::info("SwapChainHook: shutdown");
}
