#include "D3DHook.h"

#include "HookManager.h"
#include "../util/Logger.h"

#include <d3d11.h>
#include <dxgi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace glacier {

namespace {

// Finds the most recently created top-level window owned by this process. The
// throwaway swap chain we create for vtable discovery needs a valid HWND, and
// the game window is the obvious candidate.
HWND findGameWindow() {
    struct Ctx {
        DWORD pid;
        HWND  hwnd;
    } ctx{ GetCurrentProcessId(), nullptr };

    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lp);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == c->pid && GetWindow(hwnd, GW_OWNER) == nullptr && IsWindowVisible(hwnd)) {
            c->hwnd = hwnd;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.hwnd;
}

} // namespace

bool D3DHook::initialize() {
    if (m_initialized) return true;

    m_window = findGameWindow();
    if (!m_window) {
        LOG_ERROR("could not locate game window");
        return false;
    }

    // Build a dummy device + swap chain to read the real vtable pointers.
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount       = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = m_window;
    scd.SampleDesc.Count  = 1;
    scd.Windowed          = TRUE;
    scd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL obtained;

    IDXGISwapChain*      swapChain = nullptr;
    ID3D11Device*        device    = nullptr;
    ID3D11DeviceContext* context   = nullptr;

    const HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 1, D3D11_SDK_VERSION, &scd,
        &swapChain, &device, &obtained, &context);

    if (FAILED(hr) || !swapChain) {
        LOG_ERROR("dummy swapchain creation failed: {:#x}", static_cast<unsigned>(hr));
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(swapChain);

    // IDXGISwapChain vtable layout: [8] Present, [13] ResizeBuffers.
    HookManager& hooks = HookManager::get();
    bool ok = hooks.create("IDXGISwapChain::Present", vtable[8],
                           &D3DHook::hkPresent, &s_originalPresent);
    ok = hooks.create("IDXGISwapChain::ResizeBuffers", vtable[13],
                      &D3DHook::hkResizeBuffers, &s_originalResize) && ok;

    swapChain->Release();
    device->Release();
    context->Release();

    m_initialized = ok;
    return ok;
}

void D3DHook::shutdown() {
    m_initialized = false;
    s_originalPresent = nullptr;
    s_originalResize  = nullptr;
}

HRESULT STDMETHODCALLTYPE D3DHook::hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    auto& self = D3DHook::get();

    if (self.m_present) {
        ID3D11Device*        device  = nullptr;
        ID3D11DeviceContext* context = nullptr;
        if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device)))) {
            device->GetImmediateContext(&context);
            self.m_present(sc, device, context);
            if (context) context->Release();
            device->Release();
        }
    }

    return s_originalPresent(sc, sync, flags);
}

HRESULT STDMETHODCALLTYPE D3DHook::hkResizeBuffers(IDXGISwapChain* sc, UINT count, UINT w, UINT h,
                                                   DXGI_FORMAT fmt, UINT flags) {
    auto& self = D3DHook::get();
    if (self.m_resize) {
        self.m_resize(sc, w, h);
    }
    return s_originalResize(sc, count, w, h, fmt, flags);
}

} // namespace glacier
