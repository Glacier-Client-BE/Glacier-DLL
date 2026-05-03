#include "ChunkBorders.h"
#include "../../core/Glacier.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"
#include "../../sdk/SDK.h"

namespace Glacier::modules {

// Renders an indicator badge while enabled. The actual world overlay needs a
// world-space line renderer wired through RenderLevelEvent (TODO once
// LevelRenderer hook is in).
ChunkBorders::ChunkBorders()
    : Module("chunk_borders", "Chunk Borders", "Show 16x16 chunk grid", Category::Visual, 0),
      showSubChunks_(addSetting<BoolSetting>("sub_chunks", "Sub-Chunks", "16x16x16 sub-chunks too", false)),
      color_        (addSetting<ColorSetting>("color",     "Color",     "Line colour",            COL_ACCENT))
{
    EventBus::get().listen<RenderHUDEvent, &ChunkBorders::onRender>(this);
}

void ChunkBorders::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;
    // Visual indicator (top-center "active" badge).
    const char* lbl = showSubChunks_.get() ? "Chunk Borders + Sub" : "Chunk Borders";
    Draw::chip(e.drawList, ImVec2(e.width * 0.5f - 70.f, 8.f), lbl, color_.get());
}

} // namespace Glacier::modules
