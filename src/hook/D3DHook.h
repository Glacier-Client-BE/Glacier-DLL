#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_2.h>

// DirectX present/resize hook. Minecraft: Bedrock Edition's swap chain is
// hooked at the DXGI level (IDXGISwapChain::Present/ResizeBuffers), which
// works identically whether the game itself renders through D3D11 or D3D12 —
// some builds/machines use one, some the other, and there is no way to tell
// which without probing. We acquire the real vtable by spinning up a
// throwaway device + swap chain (kiero-style) rather than guessing offsets,
// then route the slots through the HookManager.
//
// D3D12 needs one more thing a D3D11 swap chain doesn't: bridging the
// overlay onto it requires a live ID3D12CommandQueue*, which is not
// reachable from the swap chain itself. It is, however, the exact object
// passed as `pDevice` to IDXGIFactory2::CreateSwapChainForHwnd when the game
// creates the swap chain — the same fact Latite's DXHooks.cpp hooks that
// call to capture it. See commandQueue().
//
// The interface is deliberately renderer-agnostic: modules and the UI only
// ever see the callbacks below, never which backend produced them.
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

    // The game's D3D12 command queue, captured off CreateSwapChainForHwnd.
    // Null on a D3D11 swap chain (nothing to capture) and null on D3D12 until
    // the game has actually created its swap chain at least once.
    ID3D12CommandQueue* commandQueue() const { return s_commandQueue; }

private:
    D3DHook() = default;

    // Detours (defined in D3DHook.cpp).
    static HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags);
    static HRESULT STDMETHODCALLTYPE hkResizeBuffers(IDXGISwapChain* sc, UINT count, UINT w, UINT h,
                                                     DXGI_FORMAT fmt, UINT flags);
    static HRESULT STDMETHODCALLTYPE hkCreateSwapChainForHwnd(
        IDXGIFactory2* factory, IUnknown* device, HWND hwnd,
        const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fsDesc,
        IDXGIOutput* output, IDXGISwapChain1** outSwapChain);

    using PresentFn       = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using CreateSwapChainForHwndFn = HRESULT(STDMETHODCALLTYPE*)(
        IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);

    inline static PresentFn       s_originalPresent = nullptr;
    inline static ResizeBuffersFn s_originalResize  = nullptr;
    inline static CreateSwapChainForHwndFn s_originalCreateSwapChainForHwnd = nullptr;
    inline static std::atomic<std::uint64_t> s_frames{ 0 };
    // Owning reference — released in shutdown(). Only ever written from
    // hkCreateSwapChainForHwnd, on the game's own thread.
    inline static ID3D12CommandQueue* s_commandQueue = nullptr;

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
