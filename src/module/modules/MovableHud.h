#pragma once

#include "../HudModule.h"

namespace glacier {

// Global HUD nudge: shifts the entire Glacier HUD layer by a configurable
// X/Y offset, for fitting the overlay around a server's own UI or a streaming
// layout. Emits a single config widget the web layer applies as a transform on
// the HUD root — one cheap CSS translate, no per-widget work.
class MovableHud final : public HudModule {
public:
    MovableHud()
        : HudModule("Movable HUD", "Reposition the whole Glacier HUD layer",
                    Category::Misc, /*positioned*/ false, "") {
        addSetting(Setting{ "offsetX", "X offset", 0, -400, 400 });
        addSetting(Setting{ "offsetY", "Y offset", 0, -400, 400 });
    }

protected:
    void writeHudBody(json::Object& o) const override {
        int ox = 0, oy = 0;
        if (const auto* s = setting("offsetX")) ox = s->asInt();
        if (const auto* s = setting("offsetY")) oy = s->asInt();
        o.set("type", "hudconfig").set("id", "hudoffset")
         .set("offsetX", ox).set("offsetY", oy);
    }
};

} // namespace glacier
