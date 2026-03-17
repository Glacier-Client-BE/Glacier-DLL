#pragma once
#include <d3d11.h>
#include <imgui.h>

class Renderer {
public:
    static Renderer& get();

    bool init(ID3D11Device* device, IDXGISwapChain* swapChain);
    void shutdown();

    void beginFrame();
    void endFrame();

    // Call before ResizeBuffers — releases the RTV so D3D can resize safely.
    void invalidateRTV();

    bool        isReady()               const { return m_ready; }
    ImDrawList* getBackgroundDrawList() const;

private:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool createRTV(IDXGISwapChain* sc);

    bool                    m_ready   = false;
    ID3D11Device*           m_device  = nullptr;
    ID3D11DeviceContext*    m_context = nullptr;
    ID3D11RenderTargetView* m_rtv     = nullptr;
};
