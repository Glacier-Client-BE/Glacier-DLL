#include "SwapChainHook.h"
#include "../HookManager.h"
#include "../../core/Logger.h"
#include "../../events/EventBus.h"
#include "../../events/Events.h"
#include "../../render/Renderer.h"
#include "WndProcHook.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace Glacier {

SwapChainHook& SwapChainHook::get() {
    static SwapChainHook S;
    return S;
}

// --- vtable resolution -------------------------------------------------------
static bool resolveSwapchainVtable(void*& outPresent, void*& outResize, HWND& outWnd) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"GlacierTmp";
    RegisterClassExW(&wc);
    HWND hWnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* sc = nullptr;

    auto hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &scd, &sc, &dev, &fl, &ctx);

    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    DestroyWindow(hWnd);

    if (FAILED(hr)) {
        Logger::get().error("DX", "Dummy device creation failed: 0x", static_cast<unsigned>(hr));
        return false;
    }

    void** vt = *reinterpret_cast<void***>(sc);
    outPresent = vt[8];   // IDXGISwapChain::Present
    outResize  = vt[13];  // IDXGISwapChain::ResizeBuffers
    outWnd     = nullptr;

    sc->Release();
    ctx->Release();
    dev->Release();
    return true;
}

bool SwapChainHook::install() {
    if (oPresent_) return true;

    void* presentTarget = nullptr;
    void* resizeTarget  = nullptr;
    HWND  ignored       = nullptr;
    if (!resolveSwapchainVtable(presentTarget, resizeTarget, ignored)) return false;

    auto& hm = HookManager::get();
    if (!hm.create("SwapChain::Present",       presentTarget, &SwapChainHook::hkPresent,       reinterpret_cast<void**>(&oPresent_))) return false;
    if (!hm.create("SwapChain::ResizeBuffers", resizeTarget,  &SwapChainHook::hkResizeBuffers, reinterpret_cast<void**>(&oResizeBuffers_))) return false;
    hm.enable("SwapChain::Present");
    hm.enable("SwapChain::ResizeBuffers");
    Logger::get().info("DX", "SwapChain hooks installed");
    return true;
}

void SwapChainHook::uninstall() {
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
    if (imguiReady_) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
    initialized_ = false;
}

void SwapChainHook::buildRenderTarget(IDXGISwapChain* sc) {
    ID3D11Texture2D* back = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) return;
    device_->CreateRenderTargetView(back, nullptr, &rtv_);
    back->Release();
}

void SwapChainHook::releaseRenderTarget() {
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
}

HRESULT __stdcall SwapChainHook::hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    auto& self = SwapChainHook::get();

    if (!self.initialized_) {
        if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&self.device_)))) {
            self.device_->GetImmediateContext(&self.context_);

            DXGI_SWAP_CHAIN_DESC scd{};
            sc->GetDesc(&scd);
            self.window_ = scd.OutputWindow;

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.IniFilename = nullptr;

            ImGui_ImplWin32_Init(self.window_);
            ImGui_ImplDX11_Init(self.device_, self.context_);
            self.imguiReady_ = true;

            // Renderer initialises fonts + theme on first Present.
            Renderer::get().onDeviceReady(self.device_, self.context_);

            // Subclass the window so we receive WM_KEYDOWN etc.
            WndProcHook::get().install(self.window_);

            self.buildRenderTarget(sc);
            self.initialized_ = true;
            Logger::get().info("DX", "First Present - Renderer up");
        }
    }

    if (self.initialized_) {
        if (!self.rtv_) self.buildRenderTarget(sc);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Renderer::get().onFrame();

        ImGui::Render();
        self.context_->OMSetRenderTargets(1, &self.rtv_, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return oPresent_(sc, sync, flags);
}

HRESULT __stdcall SwapChainHook::hkResizeBuffers(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags) {
    auto& self = SwapChainHook::get();
    self.releaseRenderTarget();
    auto hr = oResizeBuffers_(sc, n, w, h, fmt, flags);
    if (self.initialized_) self.buildRenderTarget(sc);
    return hr;
}

} // namespace Glacier
