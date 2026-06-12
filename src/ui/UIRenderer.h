#pragma once

#include <atomic>
#include <memory>
#include <d3d11.h>
#include <dxgi.h>

#include <Ultralight/Ultralight.h>

// Renders the HTML/CSS/JS interface with Ultralight into a CPU bitmap, uploads
// it to a D3D11 texture, and composites that texture over the game's back
// buffer every Present. Ultralight is chosen over WebView2 here because it
// renders off-screen to a pixel buffer we fully own — no child HWND, no
// separate process — which is exactly what an internal overlay needs.
namespace glacier {

class JsBridge;

class UIRenderer {
public:
    static UIRenderer& get() {
        static UIRenderer instance;
        return instance;
    }

    // Called once, lazily, on the first Present that has a live device.
    bool initialize(ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapChain);
    void shutdown();

    // Per-frame entry from the Present hook.
    void render(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* ctx);
    void onResize(UINT width, UINT height);

    // Toggle the click-GUI. When visible the overlay captures mouse + keyboard.
    void setMenuOpen(bool open);
    bool menuOpen() const { return m_menuOpen.load(); }

    // Input forwarding from the WndProc hook. Returns true if the overlay
    // consumed the event (so it shouldn't reach the game).
    bool onWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    ultralight::View* view() { return m_view.get(); }

private:
    UIRenderer() = default;

    void createPipeline(ID3D11Device* device);     // size-independent state
    void createSizedResources(ID3D11Device* device); // texture + srv (per size)
    void releaseSizedResources();
    void releaseDeviceObjects();
    void ensureRenderTarget(IDXGISwapChain* swapChain, ID3D11Device* device);
    void uploadAndDraw(ID3D11DeviceContext* ctx);

    // Ultralight
    ultralight::RefPtr<ultralight::Renderer> m_renderer;
    ultralight::RefPtr<ultralight::View>     m_view;
    std::unique_ptr<JsBridge>                m_bridge;

    // Non-owning device handle, cached so onResize can rebuild sized resources.
    ID3D11Device* m_device = nullptr;

    // D3D11 compositing resources
    ID3D11Texture2D*          m_texture     = nullptr;
    ID3D11ShaderResourceView* m_srv         = nullptr;
    ID3D11VertexShader*       m_vs          = nullptr;
    ID3D11PixelShader*        m_ps          = nullptr;
    ID3D11InputLayout*        m_layout      = nullptr;
    ID3D11Buffer*             m_vertexBuffer = nullptr;
    ID3D11SamplerState*       m_sampler     = nullptr;
    ID3D11BlendState*         m_blend       = nullptr;
    ID3D11RenderTargetView*   m_rtv         = nullptr;

    UINT m_width  = 0;
    UINT m_height = 0;

    unsigned long long m_lastHudPush = 0;   // GetTickCount64 of last HUD push

    std::atomic<bool> m_menuOpen   = false;
    bool              m_initialized = false;
};

} // namespace glacier
