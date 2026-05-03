#include "ClientBranding.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include <imgui.h>

namespace Glacier::modules {

ClientBranding::ClientBranding()
    : Module("branding", "Client Branding", "Subtle Glacier mark for screenshots", Category::HUD),
      opacity_(addSetting<FloatSetting>("opacity", "Opacity", "0..1", 0.55f, 0.0f, 1.0f, 0.05f))
{
    EventBus::get().listen<RenderHUDEvent, &ClientBranding::onRender>(this);
}

void ClientBranding::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s on top", kBrand);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    auto a = static_cast<unsigned>(opacity_.get() * 255.f);
    unsigned txt = (a << 24) | (COL_TEXT & 0x00FFFFFF);
    e.drawList->AddText(ImVec2(e.width - ts.x - 12.f, e.height - ts.y - 12.f),
                        Draw::argbToImU32(txt), buf);
}

} // namespace Glacier::modules
