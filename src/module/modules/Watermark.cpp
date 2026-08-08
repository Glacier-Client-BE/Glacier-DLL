#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../util/FrameStats.h"

namespace glacier {

// The simplest possible HudModule, and deliberately so: it proves the HUD
// placement/scale/background path works without depending on a single game
// signature. If the watermark draws and drags, every future HUD widget only has
// to fill in writeHudBody().
class Watermark final : public TextHudModule {
public:
    Watermark()
        : TextHudModule("Watermark", "Draws the Glacier logo on screen",
                    Category::Visual, 0,
                    0.01f, 0.01f, 0xFF4C9AFF) {
        addSetting(Setting{ "showfps", "Show FPS", true });
    }

    void buildLines(std::vector<Line>& out) override {
        // The one place SemiBold is right: this is the client's name, and the
        // whole point of a watermark is that it is the thing you read first.
        out.push_back(Line{ "Glacier", false, textColor() });

        if (const auto* s = setting("showfps"); s && s->asBool()) {
            // Shared source, so this and the FPS Counter module can never
            // disagree about the number.
            pushSecondary(out, std::to_string(FrameStats::get().fps()) + " fps");
        }
    }
};

GLACIER_MODULE(Watermark);

} // namespace glacier
