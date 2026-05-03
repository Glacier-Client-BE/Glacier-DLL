#include "Crosshair.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"

namespace Glacier::modules {

Crosshair::Crosshair()
    : Module("crosshair", "Crosshair", "Custom crosshair overlay", Category::Visual),
      style_    (addSetting<EnumSetting>("style",     "Style",     "Crosshair shape",
                     std::vector<std::string>{ "Cross", "Dot", "Circle", "T-Shape" }, 0)),
      size_     (addSetting<FloatSetting>("size",      "Size",      "Length in pixels", 8.f, 1.f, 30.f, 0.5f)),
      thickness_(addSetting<FloatSetting>("thickness", "Thickness", "Stroke width",     1.5f, 1.f, 8.f, 0.25f)),
      gap_      (addSetting<FloatSetting>("gap",       "Gap",       "Center gap",       2.f, 0.f, 12.f, 0.5f)),
      outline_  (addSetting<BoolSetting> ("outline",   "Outline",   "Add a 1px outline",true)),
      color_    (addSetting<ColorSetting>("color",     "Color",     "Stroke colour",    COL_ACCENT))
{
    EventBus::get().listen<RenderHUDEvent, &Crosshair::onRender>(this);
}

void Crosshair::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;
    float cx = e.width * 0.5f, cy = e.height * 0.5f;
    auto col = Draw::argbToImU32(color_.get());
    auto outline = Draw::argbToImU32(0xC0000000);
    float t = thickness_.get();
    float g = gap_.get();
    float s = size_.get();

    auto line = [&](ImVec2 a, ImVec2 b) {
        if (outline_.get()) e.drawList->AddLine(a, b, outline, t + 1.5f);
        e.drawList->AddLine(a, b, col, t);
    };

    switch (style_.index()) {
        case 0: // Cross
            line(ImVec2(cx - g - s, cy), ImVec2(cx - g, cy));
            line(ImVec2(cx + g,     cy), ImVec2(cx + g + s, cy));
            line(ImVec2(cx, cy - g - s), ImVec2(cx, cy - g));
            line(ImVec2(cx, cy + g),     ImVec2(cx, cy + g + s));
            break;
        case 1: // Dot
            if (outline_.get()) e.drawList->AddCircleFilled(ImVec2(cx, cy), t * 1.5f + 1.f, outline);
            e.drawList->AddCircleFilled(ImVec2(cx, cy), t * 1.5f, col);
            break;
        case 2: // Circle
            if (outline_.get()) e.drawList->AddCircle(ImVec2(cx, cy), s + 1.f, outline, 0, t + 1.f);
            e.drawList->AddCircle(ImVec2(cx, cy), s, col, 0, t);
            break;
        default: // T-shape
            line(ImVec2(cx - s, cy), ImVec2(cx + s, cy));
            line(ImVec2(cx, cy + g), ImVec2(cx, cy + g + s));
            break;
    }
}

} // namespace Glacier::modules
