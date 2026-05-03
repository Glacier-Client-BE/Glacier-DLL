#include "BlockOutline.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"

namespace Glacier::modules {

BlockOutline::BlockOutline()
    : Module("block_outline", "Block Outline", "Custom selection box style", Category::Visual),
      style_    (addSetting<EnumSetting>("style",     "Style",     "Render mode",
                     std::vector<std::string>{ "Outline", "Filled", "Both" }, 0)),
      thickness_(addSetting<FloatSetting>("thickness","Thickness", "Edge width",     2.0f, 0.5f, 6.f, 0.25f)),
      color_    (addSetting<ColorSetting>("color",    "Color",     "Edge / fill",    COL_ACCENT))
{
    EventBus::get().listen<RenderHUDEvent, &BlockOutline::onRender>(this);
}

void BlockOutline::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;
    // Without a RenderLevelEvent (world matrices), we just show a status chip
    // confirming the module is active. Real outline overrides happen inside a
    // LevelRenderer hook once Addresses are wired.
    Draw::chip(e.drawList, ImVec2(e.width * 0.5f + 60.f, 8.f), "Block Outline", color_.get());
}

} // namespace Glacier::modules
