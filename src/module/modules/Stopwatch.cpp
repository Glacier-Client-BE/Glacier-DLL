#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"
#include "../../ui/Menu.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <Windows.h>

namespace glacier {

// Manual start/pause/reset timer — distinct from Clock's automatic session
// timer, which can't be paused or zeroed and measures "since Glacier
// attached" rather than whatever the user actually wants to time.
class Stopwatch final : public TextHudModule {
public:
    Stopwatch()
        : TextHudModule("Stopwatch", "A manual start/pause/reset timer",
                    Category::Misc, 0,
                    0.01f, 0.41f, 0xFFFFFFFF) {
        setIcon(0xF2F2);   // fa-stopwatch
        addSetting(Setting{ "start", "Start/Pause key", Setting::KeyTag{}, 'P' });
        addSetting(Setting{ "reset", "Reset key", Setting::KeyTag{}, 'L' });
    }

    void onTick() override {
        // Same guard as the menu's own open/close keys: GetAsyncKeyState
        // reports the physical key regardless of window focus, so without
        // this, typing "help" in chat would start/pause the stopwatch on
        // the "p". cursorGrabbed() is false for chat, pause, and inventory,
        // and the menu being open means these keys likely mean something
        // else entirely (rebinding, typing a filter) to the user right now.
        if (!sdk::GameSDK::get().cursorGrabbed() || ui::Menu::get().open()) {
            m_startWasDown = m_resetWasDown = false;
            return;
        }

        const int startKey = settingKey("start", 'P');
        const bool startDown = (GetAsyncKeyState(startKey) & 0x8000) != 0;
        if (startDown && !m_startWasDown) {
            if (m_running) {
                m_elapsedMs += GetTickCount64() - m_startedAt;
                m_running = false;
            } else {
                m_startedAt = GetTickCount64();
                m_running = true;
            }
        }
        m_startWasDown = startDown;

        const int resetKey = settingKey("reset", 'L');
        const bool resetDown = (GetAsyncKeyState(resetKey) & 0x8000) != 0;
        if (resetDown && !m_resetWasDown && !m_running) {
            m_elapsedMs = 0;
        }
        m_resetWasDown = resetDown;
    }

    void buildLines(std::vector<Line>& out) override {
        const std::uint64_t total =
            m_elapsedMs + (m_running ? GetTickCount64() - m_startedAt : 0);
        push(out, format(total));
    }

private:
    int settingKey(std::string_view id, int fallback) const {
        const auto* s = setting(id);
        return s ? s->asInt() : fallback;
    }

    static std::string format(std::uint64_t ms) {
        const std::uint64_t totalSeconds = ms / 1000;
        const int hours = static_cast<int>(totalSeconds / 3600);
        const int mins  = static_cast<int>((totalSeconds % 3600) / 60);
        const int secs  = static_cast<int>(totalSeconds % 60);
        const int tenths = static_cast<int>((ms % 1000) / 100);

        char buf[32];
        if (hours > 0) {
            std::snprintf(buf, sizeof(buf), "%d:%02d:%02d.%d", hours, mins, secs, tenths);
        } else {
            std::snprintf(buf, sizeof(buf), "%d:%02d.%d", mins, secs, tenths);
        }
        return buf;
    }

    bool m_running = false;
    bool m_startWasDown = false;
    bool m_resetWasDown = false;
    std::uint64_t m_startedAt = 0;
    std::uint64_t m_elapsedMs = 0;
};

GLACIER_MODULE(Stopwatch);

} // namespace glacier
