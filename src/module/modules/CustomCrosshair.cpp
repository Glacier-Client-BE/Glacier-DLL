#include "../Module.h"
#include "../ModuleRegistry.h"
#include "../../ui/Renderer.h"

namespace glacier {

// A centered crosshair, drawn with the renderer's own primitives.
//
// This is a deliberately smaller feature than Flarial's CustomCrosshair,
// which loads user-supplied PNGs from disk and highlights color when a
// raycast hits an entity. Both need capability Glacier doesn't have yet: an
// image-loading pipeline, and a hit-test-result accessor. What's here is the
// part that needed neither — a plain, always-on crosshair for whenever the
// vanilla one is hidden or hard to see, which is the actual common case for
// wanting one at all.
//
// A plain Module rather than a HudModule: the position is derived from the
// screen center every frame, not a point the user drags.
class CustomCrosshair final : public Module {
public:
    CustomCrosshair()
        : Module("Custom Crosshair", "Draws a crosshair over the center of the screen",
                 Category::Visual, 0) {
        addSetting(Setting{ "size", "Arm length", 8.0f, 2.0f, 30.0f, 1.0f });
        addSetting(Setting{ "gap", "Center gap", 4.0f, 0.0f, 20.0f, 1.0f });
        addSetting(Setting{ "thickness", "Thickness", 2.0f, 1.0f, 8.0f, 0.5f });
        addSetting(Setting{ "dot", "Center dot", false });
        addSetting(Setting{ "color", "Color", Setting::ColorTag{}, 0xFFFFFFFF });
    }

    void onRender() override {
        auto& r = ui::Renderer::get();
        if (!r.ready()) return;

        const float size = settingFloat("size", 8.0f);
        const float gap = settingFloat("gap", 4.0f);
        const float thickness = settingFloat("thickness", 2.0f);
        const auto* colorSetting = setting("color");
        const ui::Color color = ui::Color::rgba(colorSetting ? colorSetting->asColor() : 0xFFFFFFFF);

        const float cx = r.width() * 0.5f;
        const float cy = r.height() * 0.5f;
        const float half = thickness * 0.5f;

        // Four separate arms rather than one plus-shaped rect, so `gap`
        // opens a real hole in the middle instead of just being padding
        // around a solid cross.
        r.fillRect(ui::Rect{ cx - half, cy - gap - size, thickness, size }, color);   // top
        r.fillRect(ui::Rect{ cx - half, cy + gap,        thickness, size }, color);   // bottom
        r.fillRect(ui::Rect{ cx - gap - size, cy - half, size, thickness }, color);   // left
        r.fillRect(ui::Rect{ cx + gap,        cy - half, size, thickness }, color);   // right

        const auto* dotSetting = setting("dot");
        if (dotSetting && dotSetting->asBool()) {
            const float d = thickness * 1.5f;
            r.fillRect(ui::Rect{ cx - d * 0.5f, cy - d * 0.5f, d, d }, color);
        }
    }

private:
    float settingFloat(std::string_view id, float fallback) const {
        const auto* s = setting(id);
        return s ? s->asFloat() : fallback;
    }
};

GLACIER_MODULE(CustomCrosshair);

} // namespace glacier
