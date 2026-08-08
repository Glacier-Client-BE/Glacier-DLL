#include "D3DHook.h"

#include "HookManager.h"
#include "../util/Logger.h"

#include <cwchar>
#include <d3d11.h>
#include <dxgi.h>
#include <iterator>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace glacier {

namespace {

// Best guess at the game window, used only until the first frame tells us for
// certain (see hkPresent). The throwaway swap chain we create for vtable
// discovery needs a valid HWND, and using the real window rather than a
// synthetic one makes it far more likely the driver hands us the same
// swap-chain implementation the game itself is using.
//
// "First top-level visible window owned by this process" is not sufficient on
// its own: Glacier allocates a debug console before this runs, and a freshly
// created console is a top-level visible window of this process that
// EnumWindows will happily return first. Console classes are therefore skipped
// explicitly — a guess that picks our own console is worse than no guess,
// because everything downstream then works on the wrong window.
bool isConsoleWindow(HWND hwnd) {
    wchar_t cls[64]{};
    if (GetClassNameW(hwnd, cls, static_cast<int>(std::size(cls))) <= 0) return false;
    return wcscmp(cls, L"ConsoleWindowClass") == 0
        || wcscmp(cls, L"PseudoConsoleWindow") == 0;
}

HWND findGameWindow() {
    struct Ctx {
        DWORD pid;
        HWND  hwnd;
    } ctx{ GetCurrentProcessId(), nullptr };

    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lp);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == c->pid && GetWindow(hwnd, GW_OWNER) == nullptr && IsWindowVisible(hwnd)
            && !isConsoleWindow(hwnd)) {
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

    // IDXGIFactory2::CreateSwapChainForHwnd — see the class comment for why
    // this one is kept despite being unreliable in practice (the game has
    // always already called it once by the time an internal DLL attaches) —
    // and IDXGIAdapter, needed below for the D3D12 probe device. One factory,
    // released once at the end: EnumAdapters below borrows it too, and
    // releasing it after the first use only would make that a
    // use-after-free.
    IDXGIFactory2* factory2 = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory2))) && factory2) {
        void** factoryVTable = *reinterpret_cast<void***>(factory2);
        hooks.create("IDXGIFactory2::CreateSwapChainForHwnd", factoryVTable[15],
                    &D3DHook::hkCreateSwapChainForHwnd, &s_originalCreateSwapChainForHwnd);

        // The one that actually works: a throwaway D3D12 device + command
        // queue, purely to read ID3D12CommandQueue's vtable — see the class
        // comment. D3D12CreateDevice failing outright just means no D3D12
        // device is available on this machine at all (the common case, since
        // most systems still run Bedrock through D3D11), not worth a warning
        // on its own; only warn if a device exists but the queue specifically
        // couldn't be made, since that is the surprising failure.
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(factory2->EnumAdapters(0, &adapter)) && adapter) {
            ID3D12Device* device12 = nullptr;
            if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12)))
                && device12) {
                D3D12_COMMAND_QUEUE_DESC qdesc{};
                qdesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
                qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                ID3D12CommandQueue* probeQueue = nullptr;
                if (SUCCEEDED(device12->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&probeQueue)))
                    && probeQueue) {
                    void** queueVTable = *reinterpret_cast<void***>(probeQueue);
                    hooks.create("ID3D12CommandQueue::ExecuteCommandLists", queueVTable[10],
                                &D3DHook::hkExecuteCommandLists, &s_originalExecuteCommandLists);
                    probeQueue->Release();
                } else {
                    LOG_WARN("could not create a probe D3D12 command queue — if the game "
                             "renders through D3D12, the overlay will not appear (no command "
                             "queue to bridge with)");
                }
                device12->Release();
            }
            adapter->Release();
        }

        factory2->Release();
    }

    m_initialized = ok;
    return ok;
}

void D3DHook::shutdown() {
    m_initialized = false;
    s_originalPresent = nullptr;
    s_originalResize  = nullptr;
    s_originalCreateSwapChainForHwnd = nullptr;
    s_originalExecuteCommandLists    = nullptr;
    s_hookedVTable    = nullptr;
    s_vtableChecked.store(false, std::memory_order_relaxed);
    if (s_commandQueue) {
        s_commandQueue->Release();
        s_commandQueue = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE D3DHook::hkCreateSwapChainForHwnd(
        IDXGIFactory2* factory, IUnknown* device, HWND hwnd,
        const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fsDesc,
        IDXGIOutput* output, IDXGISwapChain1** outSwapChain) {
    // `device` is an ID3D12CommandQueue* when the caller is creating a D3D12
    // swap chain (an ID3D11Device* for D3D11) — DXGI overloads the parameter
    // by which interface the caller happens to pass. QueryInterface is the
    // correct way to tell which, rather than assuming from context.
    if (device && !s_commandQueue) {
        ID3D12CommandQueue* queue = nullptr;
        if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D12CommandQueue),
                                             reinterpret_cast<void**>(&queue))) && queue) {
            s_commandQueue = queue;   // owning ref, released in shutdown()
            LOG_INFO("captured the game's D3D12 command queue — the overlay can bridge onto "
                     "its swap chain via D3D11On12 if needed");
        }
    }
    return s_originalCreateSwapChainForHwnd(factory, device, hwnd, desc, fsDesc, output, outSwapChain);
}

void STDMETHODCALLTYPE D3DHook::hkExecuteCommandLists(
        ID3D12CommandQueue* self, UINT numLists, ID3D12CommandList* const* lists) {
    // The real, reliable capture path — see the class comment on why
    // hkCreateSwapChainForHwnd above almost never actually catches anything.
    // Guards against grabbing a transient/secondary queue (compute, copy,
    // bundle-only) by requiring DIRECT — the only type usable for
    // D3D11On12CreateDevice's presentation bridge anyway.
    if (self && !s_commandQueue) {
        const D3D12_COMMAND_QUEUE_DESC desc = self->GetDesc();
        if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            self->AddRef();   // owning ref, released in shutdown()
            s_commandQueue = self;
            LOG_INFO("captured the game's D3D12 command queue via ExecuteCommandLists — the "
                     "overlay can bridge onto its swap chain via D3D11On12");
        }
    }
    s_originalExecuteCommandLists(self, numLists, lists);
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

    // The swap chain knows which window it presents to, and that is the only
    // authoritative answer. Guessing it by enumerating the process's top-level
    // windows picked up the debug console instead — which hooked the WndProc on
    // the wrong window (no menu interaction), converted cursor coordinates
    // against the wrong client origin (hover landing on the wrong widget), and
    // retitled the console rather than the game.
    if (!s_windowConfirmed.exchange(true, std::memory_order_relaxed)) {
        DXGI_SWAP_CHAIN_DESC desc{};
        if (SUCCEEDED(sc->GetDesc(&desc)) && desc.OutputWindow) {
            if (desc.OutputWindow != self.m_window) {
                LOG_INFO("game window corrected to {:#x} (guessed {:#x} before the first frame)",
                         reinterpret_cast<std::uintptr_t>(desc.OutputWindow),
                         reinterpret_cast<std::uintptr_t>(self.m_window));
            }
            self.m_window = desc.OutputWindow;
            if (self.m_windowResolved) self.m_windowResolved(desc.OutputWindow);
        } else {
            LOG_WARN("could not read the swapchain's output window — input and window "
                     "branding stay on the guessed window");
        }
    }

    if (self.m_present) {
        // GetDevice(ID3D11Device) used to gate this whole call — nothing
        // rendered at all if it failed, which it always does on a swap chain
        // actually backed by D3D12 (GetDevice(IID_ID3D11Device) on a D3D12
        // swap chain correctly returns E_NOINTERFACE; that is not an error,
        // it is DXGI telling the truth). The callback never even used
        // `device`/`context` for anything — ui::Renderer::beginFrame binds
        // straight to the swap chain's back buffer — so the gate was pure
        // liability: silent, total loss of the overlay on any machine or
        // Bedrock build using the D3D12 renderer, with nothing logged.
        // Called unconditionally now; device/context are best-effort extras
        // for anything that might want them later, not a precondition.
        ID3D11Device*        device  = nullptr;
        ID3D11DeviceContext* context = nullptr;
        if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) && device) {
            device->GetImmediateContext(&context);
        }
        self.m_present(sc, device, context);
        if (context) context->Release();
        if (device) device->Release();
    }

    return s_originalPresent(sc, sync, flags);
}

HRESULT STDMETHODCALLTYPE D3DHook::hkResizeBuffers(IDXGISwapChain* sc, UINT count, UINT w, UINT h,
                                                   DXGI_FORMAT fmt, UINT flags) {
    auto& self = D3DHook::get();

    // Before the original, always: the callback's job is to drop every
    // reference we hold to the back buffer, and ResizeBuffers fails outright if
    // one is still outstanding. Note w/h of 0 is legal and means "keep the
    // current size" — the callback must handle that, not skip it.
    LOG_TRACE("ResizeBuffers({} buffers, {}x{}) — releasing our back-buffer references",
              count, w, h);
    if (self.m_resize) {
        self.m_resize(sc, w, h);
    }

    const HRESULT hr = s_originalResize(sc, count, w, h, fmt, flags);
    if (FAILED(hr)) {
        // If this ever fires, the game is now running on a swap chain it thinks
        // it resized and did not. It will crash shortly afterwards, somewhere
        // that looks nothing like the cause — so name it here.
        LOG_ERROR("the game's ResizeBuffers FAILED ({:#x}). If this is "
                  "DXGI_ERROR_INVALID_CALL (0x887A0001), Glacier is still holding a "
                  "back-buffer reference and that is our bug.",
                  static_cast<unsigned>(hr));
    }
    return hr;
}

} // namespace glacier
