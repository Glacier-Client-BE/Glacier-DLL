#include "../HudModule.h"
#include "../ModuleRegistry.h"

#include <deque>
#include <mutex>
#include <string>

namespace glacier {

// Clicks-per-second readout.
//
// Click timestamps arrive on the window thread (via Module::onClick from the
// WndProc hook) and are read on the render thread, so the deques are guarded.
// A sliding one-second window is used rather than a per-second bucket counter:
// buckets make the number jump discontinuously at each boundary, which reads as
// a broken counter when you are clicking steadily.
//
// No game memory involved.
class CpsCounter final : public HudModule {
public:
    CpsCounter()
        : HudModule("CPS Counter", "Shows clicks per second",
                    Category::Combat, 0,
                    0.01f, 0.09f, 0xFFFFFFFF) {
        addSetting(Setting{ "right", "Show right clicks", true });
    }

    void onClick(bool rightButton) override {
        const ULONGLONG now = GetTickCount64();
        std::scoped_lock lock(m_mutex);
        (rightButton ? m_right : m_left).push_back(now);
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        auto& r = ui::Renderer::get();

        const bool showRight = settingBool("right", true);
        std::string text;
        {
            const ULONGLONG now = GetTickCount64();
            std::scoped_lock lock(m_mutex);
            prune(m_left, now);
            prune(m_right, now);

            text = std::to_string(m_left.size()) + " CPS";
            if (showRight) {
                text += "  |  " + std::to_string(m_right.size()) + " RCPS";
            }
        }

        const float size = 15.0f * scale;
        const float w = r.measureText(text, size, true);
        const float h = size * 1.4f;

        r.drawText(text, ui::Rect{ origin.x, origin.y, w + 4.0f, h },
                   textColor(), size, ui::TextAlign::Left, true);

        return ui::Rect{ origin.x, origin.y, w, h };
    }

private:
    // Drops everything older than the one-second window. Called under the lock.
    static void prune(std::deque<ULONGLONG>& q, ULONGLONG now) {
        while (!q.empty() && now - q.front() >= 1000) {
            q.pop_front();
        }
    }

    std::mutex m_mutex;
    std::deque<ULONGLONG> m_left;
    std::deque<ULONGLONG> m_right;
};

GLACIER_MODULE(CpsCounter);

} // namespace glacier
