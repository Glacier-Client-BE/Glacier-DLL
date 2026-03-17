#include "SwapChainHook.h"
#include "HookManager.h"
#include "../render/Renderer.h"
#include "../render/ModMenu.h"
#include "../modules/ModuleManager.h"
#include "../utils/Logger.h"
#include "../utils/ClientConfig.h"
#include "../sdk/ClientInstance.h"
#include <imgui_impl_win32.h>
#include <dxgi.h>

// ── Typedefs ─────────────────────────────────────────────────────────────────
using PresentFn      = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn= HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

// ── Original function pointers ────────────────────────────────────────────────
static PresentFn       g_originalPresent      = nullptr;
static ResizeBuffersFn g_originalResizeBuffers = nullptr;

// WndProc chain
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static WNDPROC g_originalWndProc = nullptr;

static LRESULT CALLBACK hookedWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;

    // Toggle menu key
    if (msg == WM_KEYDOWN && (int)wp == ClientConfig::get().menuKey) {
        ModMenu::get().toggle();
        return 0;
    }

    // Block mouse input from reaching the game when menu is open
    if (ModMenu::get().isOpen()) {
        switch (msg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            case WM_MOUSEMOVE:   case WM_MOUSEWHEEL:
                return 0;
        }
    }

    return CallWindowProcA(g_originalWndProc, hwnd, msg, wp, lp);
}

// ── Present hook ──────────────────────────────────────────────────────────────
static HRESULT STDMETHODCALLTYPE hookedPresent(IDXGISwapChain* sc, UINT syncInterval, UINT flags) {
    static bool s_init = false;

    if (!s_init) {
        ID3D11Device* device = nullptr;
        sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device));
        if (device) {
            if (Renderer::get().init(device, sc)) {
                ModMenu::get().init();

                // Hook WndProc
                DXGI_SWAP_CHAIN_DESC desc{};
                sc->GetDesc(&desc);
                g_originalWndProc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrA(desc.OutputWindow, GWLP_WNDPROC,
                                      reinterpret_cast<LONG_PTR>(hookedWndProc)));

                s_init = true;
                Logger::info("SwapChainHook: renderer and WndProc hooked");
            }
            device->Release();
        }
    }

    if (Renderer::get().isReady()) {
        // Tick all modules
        ModuleManager::get().onTick();

        // Update runtime trackers
        auto* lp = getLocalPlayer();
        TargetTracker::get().update(lp);

        Renderer::get().beginFrame();

        // Render ImGui overlay
        ImDrawList* dl = Renderer::get().getBackgroundDrawList();
        if (dl) ModuleManager::get().onRender(dl);

        ModuleManager::get().onRenderImGui();
        ModMenu::get().render();

        Renderer::get().endFrame();
    }

    return g_originalPresent(sc, syncInterval, flags);
}

// ── ResizeBuffers hook ────────────────────────────────────────────────────────
static HRESULT STDMETHODCALLTYPE hookedResizeBuffers(
    IDXGISwapChain* sc, UINT bc, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags)
{
    Renderer::get().invalidateRTV();
    return g_originalResizeBuffers(sc, bc, w, h, fmt, flags);
}

// ── SwapChainHook impl ────────────────────────────────────────────────────────
SwapChainHook& SwapChainHook::get() {
    static SwapChainHook instance;
    return instance;
}

bool SwapChainHook::init() {
    // Create a temporary D3D device + swap chain to grab the vtable
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount        = 1;
    sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow       = GetForegroundWindow();
    sd.SampleDesc.Count   = 1;
    sd.Windowed           = TRUE;
    sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device*        pDevice    = nullptr;
    ID3D11DeviceContext* pContext   = nullptr;
    IDXGISwapChain*      pSwapChain = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &pSwapChain, &pDevice, nullptr, &pContext);

    if (FAILED(hr)) {
        Logger::error("SwapChainHook: D3D11CreateDeviceAndSwapChain failed: 0x{:08X}",
                      static_cast<uint32_t>(hr));
        return false;
    }

    // Vtable slots: Present = 8, ResizeBuffers = 13
    void** vtable = *reinterpret_cast<void***>(pSwapChain);

    HookManager::get().install(vtable[8],  reinterpret_cast<void*>(hookedPresent),
                               reinterpret_cast<void**>(&g_originalPresent));
    HookManager::get().install(vtable[13], reinterpret_cast<void*>(hookedResizeBuffers),
                               reinterpret_cast<void**>(&g_originalResizeBuffers));
    HookManager::get().enableAll();

    pSwapChain->Release();
    pDevice->Release();
    pContext->Release();

    Logger::info("SwapChainHook: Present and ResizeBuffers hooked");
    return true;
}

void SwapChainHook::shutdown() {
    // WndProc is unhooked when renderer shuts down (window may already be gone)
    if (g_originalWndProc) {
        // Best-effort: if the window is still alive restore the original proc
        DXGI_SWAP_CHAIN_DESC desc{};
        // We don't have the swap chain pointer here, so skip restore
        g_originalWndProc = nullptr;
    }
    Logger::info("SwapChainHook: shutdown");
}
