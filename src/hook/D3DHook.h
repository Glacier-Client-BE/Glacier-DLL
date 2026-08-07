#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <d3d11.h>
#include <dxgi.h>

// DirectX 11 present/resize hook. Minecraft: Bedrock Edition renders through a
// DXGI swap chain backed by D3D11, so the client overlay is driven from inside
// IDXGISwapChain::Present. We acquire the real vtable by spinning up a throwaway
// device + swap chain (kiero-style) rather than guessing offsets, then route the
// slots through the HookManager.
//
// The interface is deliberately renderer-agnostic: modules and the UI only ever
// see the callbacks below, never the D3D11 specifics of how they were obtained.
// A DX12 backend can therefore be added later by producing the same two events
// without touching a line of module code.
namespace glacier {

class D3DHook {
public:
    // Callbacks the renderer registers. onPresent fires every frame with the
    // live device/context/swapchain; onResize fires when the back buffer is
    // re-created so the overlay can drop its stale render targets.
    using PresentCallback = std::function<void(IDXGISwapChain*, ID3D11Device*, ID3D11DeviceContext*)>;
    using ResizeCallback  = std::function<void(IDXGISwapChain*, UINT width, UINT height)>;

    static D3DHook& get() {
        static D3DHook instance;
        return instance;
    }

    bool initialize();
    void shutdown();

    // Fires once, on the first real frame, with the window the game's swap chain
    // actually presents to. Until then window() is only a guess — see the
    // comment at the call site in hkPresent.
    using WindowCallback = std::function<void(HWND)>;

    void onPresent(PresentCallback cb) { m_present = std::move(cb); }
    void onResize(ResizeCallback cb)   { m_resize = std::move(cb); }
    void onWindowResolved(WindowCallback cb) { m_windowResolved = std::move(cb); }

    HWND window() const { return m_window; }

    // Frames observed through the detour. Zero after several seconds means the
    // hook was installed on a vtable the game does not actually use — a silent
    // failure otherwise, so startup checks this and says so out loud.
    std::uint64_t framesSeen() const { return s_frames.load(std::memory_order_relaxed); }

private:
    D3DHook() = default;

    // Detours (defined in D3DHook.cpp).
    static HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags);
    static HRESULT STDMETHODCALLTYPE hkResizeBuffers(IDXGISwapChain* sc, UINT count, UINT w, UINT h,
                                                     DXGI_FORMAT fmt, UINT flags);

    using PresentFn       = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    inline static PresentFn       s_originalPresent = nullptr;
    inline static ResizeBuffersFn s_originalResize  = nullptr;
    inline static std::atomic<std::uint64_t> s_frames{ 0 };

    // The vtable we hooked, recorded so the first live frame can confirm the
    // game's swap chain really uses it (see the mismatch check in hkPresent).
    inline static void** s_hookedVTable = nullptr;
    inline static std::atomic<bool> s_vtableChecked{ false };
    inline static std::atomic<bool> s_windowConfirmed{ false };

    PresentCallback m_present;
    ResizeCallback  m_resize;
    WindowCallback  m_windowResolved;
    HWND            m_window = nullptr;
    bool            m_initialized = false;
};

} // namespace glacier
