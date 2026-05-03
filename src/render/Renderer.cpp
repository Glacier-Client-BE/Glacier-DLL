#include "Renderer.h"
#include "Theme.h"
#include "Fonts.h"
#include "../core/Logger.h"
#include "../events/EventBus.h"
#include "../events/Events.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

namespace Glacier {

Renderer& Renderer::get() {
    static Renderer R;
    return R;
}

void Renderer::onDeviceReady(ID3D11Device* d, ID3D11DeviceContext* c) {
    device_  = d;
    context_ = c;

    if (!themed_) {
        Theme::apply();
        Fonts::get().load();
        // The DX11 backend keeps a font texture cached; rebuild after we
        // changed the atlas above.
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();
        themed_ = true;
        Logger::get().info("Render", "Theme + fonts ready");
    }
}

void Renderer::onFrame() {
    auto& io = ImGui::GetIO();

    // 1) ImGui windows (click-GUI etc.)
    {
        RenderImGuiEvent ev;
        EventBus::get().dispatch(ev);
    }

    // 2) HUD overlays drawn directly on the foreground draw list.
    {
        RenderHUDEvent ev;
        ev.drawList = ImGui::GetForegroundDrawList();
        ev.width    = io.DisplaySize.x;
        ev.height   = io.DisplaySize.y;
        EventBus::get().dispatch(ev);
    }
}

void Renderer::onResize() {
    // SwapChainHook handles render-target rebuild.
}

} // namespace Glacier
