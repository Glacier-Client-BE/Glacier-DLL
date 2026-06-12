#pragma once

#include "../HudModule.h"

namespace glacier {

// Always-on client watermark. Pure text, no game data — corner-docked.
class Watermark final : public HudModule {
public:
    Watermark()
        : HudModule("Watermark", "Shows the Glacier client name and version",
                    Category::Misc, /*positioned*/ false, "top-left") {}

protected:
    void writeHudBody(json::Object& o) const override {
        o.set("type", "text").set("id", "watermark").set("anchor", anchor())
         .set("label", "").set("value", "Glacier").set("sub", "v1.0 · MCBE 1.26");
    }
};

} // namespace glacier
