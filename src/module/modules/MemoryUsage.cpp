#include "../TextHudModule.h"
#include "../ModuleRegistry.h"

#include <cstdio>
#include <string>
#include <Windows.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

namespace glacier {

// How much memory Minecraft is using, after Flarial's `Memory` module.
//
// The only widget in the catalog that reads nothing from the game at all —
// it asks the OS about the process it happens to be injected into. That makes
// it immune to every Bedrock update, and it is genuinely the readout people
// want when the game starts stuttering after an hour in a big world.
//
// Working set rather than private/commit bytes: it is what Task Manager's
// "Memory" column shows, so the number here matches the number a user would
// check it against. A figure that disagrees with Task Manager is worse than
// no figure, however defensible the definition behind it.
class MemoryUsage final : public TextHudModule {
public:
    MemoryUsage()
        : TextHudModule("Memory", "Shows the game's memory usage",
                    Category::Misc, 0,
                    0.01f, 0.49f, 0xFFFFFFFF) {
        setIcon(0xF2DB);   // fa-microchip
        addSetting(Setting{ "label", "Show \"RAM\" label", true });
        addSetting(Setting{ "system", "Show share of system RAM", false });
    }

    void buildLines(std::vector<Line>& out) override {
        PROCESS_MEMORY_COUNTERS counters{};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
            // Can only really fail if the process handle is being torn down.
            // Say nothing rather than showing a zero that reads as a real
            // measurement.
            return;
        }

        const double megabytes = static_cast<double>(counters.WorkingSetSize) / (1024.0 * 1024.0);

        char buf[48];
        if (settingBool("label", true)) {
            std::snprintf(buf, sizeof(buf), "RAM %.0f MB", megabytes);
        } else {
            std::snprintf(buf, sizeof(buf), "%.0f MB", megabytes);
        }
        push(out, buf);

        if (settingBool("system", false)) {
            MEMORYSTATUSEX status{};
            status.dwLength = sizeof(status);
            if (GlobalMemoryStatusEx(&status) && status.ullTotalPhys > 0) {
                const double share = static_cast<double>(counters.WorkingSetSize)
                                   / static_cast<double>(status.ullTotalPhys) * 100.0;
                std::snprintf(buf, sizeof(buf), "%.1f%% of system", share);
                pushSecondary(out, buf);
            }
        }
    }
};

GLACIER_MODULE(MemoryUsage);

} // namespace glacier
