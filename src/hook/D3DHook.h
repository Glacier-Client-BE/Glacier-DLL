#pragma once

#include <cstdint>
#include <functional>
#include <d3d11.h>
#include <dxgi.h>

// DirectX 11 present/resize hook. Minecraft: Bedrock Edition renders through a
// DXGI swap chain backed by D3D11, so the client overlay is driven from inside
// IDXGISwapChain::Present. We acquire the real vtable by spinning up a throwaway
// device + swap chain (kiero-style) rather than guessing offsets, then route
// the slots through the HookManager.
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

    void onPresent(PresentCallback cb) { m_present = std::move(cb); }
    void onResize(ResizeCallback cb)   { m_resize = std::move(cb); }

    // Set by the renderer when it wants raw access to WndProc (input capture).
    HWND window() const { return m_window; }

private:
    D3DHook() = default;

    // Detours (defined in D3DHook.cpp).
    static HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags);
    static HRESULT STDMETHODCALLTYPE hkResizeBuffers(IDXGISwapChain* sc, UINT count, UINT w, UINT h,
                                                     DXGI_FORMAT fmt, UINT flags);

    using PresentFn      = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    inline static PresentFn        s_originalPresent = nullptr;
    inline static ResizeBuffersFn  s_originalResize  = nullptr;

    PresentCallback m_present;
    ResizeCallback  m_resize;
    HWND            m_window = nullptr;
    bool            m_initialized = false;
};

} // namespace glacier
