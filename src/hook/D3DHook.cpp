#include "D3DHook.h"

#include "HookManager.h"
#include "../util/Logger.h"

#include <d3d11.h>
#include <dxgi.h>
#include <iterator>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace glacier {

namespace {

// Finds the top-level visible window owned by this process. The throwaway swap
// chain we create for vtable discovery needs a valid HWND, and the game window
// is the obvious candidate — using the real window (rather than a synthetic
// one) also makes it far more likely the driver hands us the same swap-chain
// implementation the game itself is using.
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

// Creates a throwaway device + swap chain purely to read vtable pointers off a
// real DXGI object. Tries the hardware driver first and falls back to WARP: on
// machines where the GPU is already saturated or the driver refuses a second
// device, hardware creation can fail even though the game is rendering fine,
// and WARP's IDXGISwapChain vtable is the same layout either way.
bool createProbe(HWND window,
                 IDXGISwapChain** outSwapChain,
                 ID3D11Device** outDevice,
                 ID3D11DeviceContext** outContext) {
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount       = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = window;
    scd.SampleDesc.Count  = 1;
    scd.Windowed          = TRUE;
    scd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    constexpr D3D_DRIVER_TYPE drivers[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP };

    for (const auto driver : drivers) {
        D3D_FEATURE_LEVEL obtained{};
        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, driver, nullptr, 0,
            levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION, &scd,
            outSwapChain, outDevice, &obtained, outContext);

        if (SUCCEEDED(hr) && *outSwapChain) {
            if (driver == D3D_DRIVER_TYPE_WARP) {
                LOG_WARN("hardware probe device failed; fell back to WARP for vtable discovery");
            }
            return true;
        }
        LOG_WARN("probe device creation failed (driver {}): {:#x}",
                 driver == D3D_DRIVER_TYPE_HARDWARE ? "hardware" : "warp",
                 static_cast<unsigned>(hr));
    }
    return false;
}

} // namespace

bool D3DHook::initialize() {
    if (m_initialized) return true;

    m_window = findGameWindow();
    if (!m_window) {
        LOG_ERROR("could not locate game window");
        return false;
    }

    IDXGISwapChain*      swapChain = nullptr;
    ID3D11Device*        device    = nullptr;
    ID3D11DeviceContext* context   = nullptr;

    if (!createProbe(m_window, &swapChain, &device, &context)) {
        LOG_ERROR("could not create a probe swapchain — D3D hook unavailable");
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(swapChain);
    s_hookedVTable = vtable;

    // IDXGISwapChain vtable layout: [8] Present, [13] ResizeBuffers.
    HookManager& hooks = HookManager::get();
    bool ok = hooks.create("IDXGISwapChain::Present", vtable[8],
                           &D3DHook::hkPresent, &s_originalPresent);
    ok = hooks.create("IDXGISwapChain::ResizeBuffers", vtable[13],
                      &D3DHook::hkResizeBuffers, &s_originalResize) && ok;

    if (context) context->Release();
    if (device) device->Release();
    if (swapChain) swapChain->Release();

    m_initialized = ok;
    return ok;
}

void D3DHook::shutdown() {
    m_initialized = false;
    s_originalPresent = nullptr;
    s_originalResize  = nullptr;
    s_hookedVTable    = nullptr;
    s_vtableChecked.store(false, std::memory_order_relaxed);
}

HRESULT STDMETHODCALLTYPE D3DHook::hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    auto& self = D3DHook::get();

    s_frames.fetch_add(1, std::memory_order_relaxed);

    // One-time sanity check on the first real frame: confirm the game's swap
    // chain dispatches through the same vtable our probe device handed us. If a
    // driver shim or overlay (Discord, Steam, RTSS…) has swapped in a different
    // implementation, we are hooked but potentially on the wrong object — say so
    // once rather than leaving a silent misbehaviour to debug later.
    if (!s_vtableChecked.exchange(true, std::memory_order_relaxed)) {
        void** live = *reinterpret_cast<void***>(sc);
        if (live != s_hookedVTable) {
            LOG_WARN("live swapchain vtable {:#x} differs from the probed vtable {:#x} — "
                     "another overlay is likely present; Glacier is still rendering "
                     "because this detour is running",
                     reinterpret_cast<std::uintptr_t>(live),
                     reinterpret_cast<std::uintptr_t>(s_hookedVTable));
        } else {
            LOG_INFO("D3D hook confirmed live on the game's swapchain");
        }
    }

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
