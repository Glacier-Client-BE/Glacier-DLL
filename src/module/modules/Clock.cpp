#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../util/FrameStats.h"

#include <cstdio>
#include <ctime>
#include <string>

namespace glacier {

// Real-world clock and session length. Touches no game memory.
class Clock final : public TextHudModule {
public:
    Clock()
        : TextHudModule("Clock", "Shows the real time and session length",
                    Category::Misc, 0,
                    0.01f, 0.29f, 0xFFFFFFFF) {
        addSetting(Setting{ "seconds", "Show seconds", false });
        addSetting(Setting{ "24h", "24-hour clock", true });
        addSetting(Setting{ "session", "Show session length", true });
    }

    void buildLines(std::vector<Line>& out) override {
        push(out, wallClock());
        // Session length is supporting detail, so it gets its own dimmer line
        // rather than being parenthesised onto the clock. Two clean lines read
        // faster at a glance than one long one.
        if (settingBool("session", true)) {
            pushSecondary(out, sessionLength());
        }
    }

private:
    std::string wallClock() const {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        // localtime_s, not localtime: the latter returns a shared buffer and
        // this runs on the render thread alongside anything else in the process.
        if (localtime_s(&local, &now) != 0) return "--:--";

        const bool h24 = settingBool("24h", true);
        int hour = local.tm_hour;
        const char* suffix = "";
        if (!h24) {
            suffix = (hour >= 12) ? " PM" : " AM";
            hour = hour % 12;
            if (hour == 0) hour = 12;
        }

        char buf[32];
        if (settingBool("seconds", false)) {
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d%s",
                          hour, local.tm_min, local.tm_sec, suffix);
        } else {
            std::snprintf(buf, sizeof(buf), "%02d:%02d%s", hour, local.tm_min, suffix);
        }
        return buf;
    }

    static std::string sessionLength() {
        const int total = FrameStats::get().sessionSeconds();
        char buf[32];
        if (total >= 3600) {
            std::snprintf(buf, sizeof(buf), "%dh %02dm", total / 3600, (total % 3600) / 60);
        } else {
            std::snprintf(buf, sizeof(buf), "%dm %02ds", total / 60, total % 60);
        }
        return buf;
    }
};

GLACIER_MODULE(Clock);

} // namespace glacier
