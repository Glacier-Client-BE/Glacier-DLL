#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../util/FrameStats.h"

#include <cstdio>
#include <string>

namespace glacier {

// Milliseconds per frame — the same number FPS Counter's fps() is derived
// from, just expressed the way a frame-pacing complaint usually is ("it
// spiked to 40ms") rather than the way a marketing number is ("only 25fps").
class FrameTimeDisplay final : public TextHudModule {
public:
    FrameTimeDisplay()
        : TextHudModule("Frame Time", "Shows milliseconds per frame",
                    Category::Misc, 0,
                    0.01f, 0.07f, 0xFFFFFFFF) {}

    void buildLines(std::vector<Line>& out) override {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.1f ms", FrameStats::get().frameTimeMs());
        push(out, buf);
    }
};

GLACIER_MODULE(FrameTimeDisplay);

} // namespace glacier
