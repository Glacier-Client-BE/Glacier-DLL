#pragma once
#include <d3d11.h>

namespace Glacier {

// Owns the DX11 device pointers passed up from SwapChainHook, runs ImGui's
// per-frame pass, and dispatches RenderHUDEvent / RenderImGuiEvent.
class Renderer {
public:
    static Renderer& get();

    void onDeviceReady(ID3D11Device* device, ID3D11DeviceContext* context);
    void onFrame();
    void onResize();

    ID3D11Device*        device()  const { return device_; }
    ID3D11DeviceContext* context() const { return context_; }

private:
    Renderer() = default;
    ID3D11Device*        device_  = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    bool                 themed_  = false;
};

} // namespace Glacier
