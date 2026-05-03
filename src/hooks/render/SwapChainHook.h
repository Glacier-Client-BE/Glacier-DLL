#pragma once
//
// DX11 swap-chain hook. Hooks IDXGISwapChain::Present + ResizeBuffers via
// vtable swap (a dummy device is created to obtain the vtable, then MinHook
// installs trampolines on the function addresses).
//
// On the first Present call the hook initialises ImGui's DX11/Win32 backends
// and installs WndProcHook; subsequent calls dispatch RenderHUDEvent and
// RenderImGuiEvent.
//
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

namespace Glacier {

class SwapChainHook {
public:
    static SwapChainHook& get();

    // Resolve the swapchain vtable using a dummy device + window, then install
    // hooks via the global HookManager.
    bool install();
    void uninstall();

    // Accessors used by the renderer / WndProcHook.
    ID3D11Device*        device()  const { return device_; }
    ID3D11DeviceContext* context() const { return context_; }
    HWND                 window()  const { return window_; }

private:
    SwapChainHook() = default;
    ~SwapChainHook() { uninstall(); }

    static HRESULT __stdcall hkPresent      (IDXGISwapChain* sc, UINT sync, UINT flags);
    static HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags);

    using PresentFn       = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    inline static PresentFn       oPresent_       = nullptr;
    inline static ResizeBuffersFn oResizeBuffers_ = nullptr;

    bool                  initialized_ = false;
    bool                  imguiReady_  = false;
    ID3D11Device*         device_      = nullptr;
    ID3D11DeviceContext*  context_     = nullptr;
    ID3D11RenderTargetView* rtv_       = nullptr;
    HWND                  window_      = nullptr;

    void buildRenderTarget(IDXGISwapChain* sc);
    void releaseRenderTarget();
};

} // namespace Glacier
