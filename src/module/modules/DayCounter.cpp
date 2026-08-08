#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <cstdio>
#include <string>

namespace glacier {

// In-world day number and time of day, derived from the absolute tick count.
//
// A Minecraft day is 24000 ticks, with 0 at sunrise. The tick count comes from
// a read-only hook on the game's own time-of-day helper, so this shows nothing
// until the game has computed the time at least once — which is correct
// behaviour on the main menu, not a bug.
class DayCounter final : public TextHudModule {
public:
    DayCounter()
        : TextHudModule("Day Counter", "Shows the in-world day and time",
                    Category::World, 0,
                    0.01f, 0.21f, 0xFFFFFFFF) {
        addSetting(Setting{ "clock", "Show time of day", true });
        addSetting(Setting{ "phase", "Show day/night phase", false });
    }

    void buildLines(std::vector<Line>& out) override {
        const auto ticks = sdk::GameSDK::get().worldTime();

        if (!ticks) {
            out.push_back(Line{ "Day --", false, textColor().withAlpha(0.45f) });
            return;
        }

        const int day   = *ticks / 24000;
        const int ofDay = *ticks % 24000;
        push(out, "Day " + std::to_string(day));

        std::string detail;
        if (settingBool("clock", true)) {
            // Tick 0 is 06:00 in-world, not midnight.
            const int minutes = static_cast<int>((static_cast<long long>(ofDay) * 1440 / 24000 + 360) % 1440);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%02d:%02d", minutes / 60, minutes % 60);
            detail = buf;
        }
        if (settingBool("phase", false)) {
            // Mobs spawn from 13000 to 23000.
            if (!detail.empty()) detail += "  ";
            detail += (ofDay >= 13000 && ofDay < 23000) ? "Night" : "Day";
        }
        if (!detail.empty()) pushSecondary(out, detail);
    }
};

GLACIER_MODULE(DayCounter);

} // namespace glacier
