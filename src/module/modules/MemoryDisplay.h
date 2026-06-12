#pragma once

#include <format>
#include <Windows.h>

#define PSAPI_VERSION 2
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

#include "../HudModule.h"

namespace glacier {

// Live RAM readout: the game process's working set plus (optionally) system-
// wide memory pressure. Queried on a 500 ms throttle from the render fan-out —
// the OS calls are cheap but there's no reason to make them 200×/s.
class MemoryDisplay final : public HudModule {
public:
    MemoryDisplay()
        : HudModule("Memory Display", "Show RAM usage on screen", Category::Misc,
                    /*positioned*/ false, "top-right") {
        addSetting(Setting{ "system", "Show system usage", true });
    }

    void onRender() override {
        const unsigned long long nowMs = GetTickCount64();
        if (nowMs - m_lastQuery < 500) return;
        m_lastQuery = nowMs;

        PROCESS_MEMORY_COUNTERS pmc{};
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            m_processMb = static_cast<int>(pmc.WorkingSetSize / (1024ull * 1024ull));
        }

        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) {
            m_systemPercent = static_cast<int>(ms.dwMemoryLoad);
        }
    }

protected:
    void writeHudBody(json::Object& o) const override {
        o.set("type", "text").set("id", "memory").set("anchor", anchor())
         .set("label", "RAM")
         .set("value", m_processMb >= 0 ? std::format("{} MB", m_processMb) : "—");
        const auto* sys = setting("system");
        if (sys && sys->asBool() && m_systemPercent >= 0) {
            o.set("sub", std::format("system {}%", m_systemPercent));
        }
    }

private:
    unsigned long long m_lastQuery = 0;
    int m_processMb     = -1;
    int m_systemPercent = -1;
};

} // namespace glacier
