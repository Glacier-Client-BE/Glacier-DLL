#include "../TextHudModule.h"
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
class CpsCounter final : public TextHudModule {
public:
    CpsCounter()
        : TextHudModule("CPS Counter", "Shows clicks per second",
                    Category::Combat, 0,
                    0.01f, 0.09f, 0xFFFFFFFF) {
        addSetting(Setting{ "right", "Show right clicks", true });
    }

    void onClick(bool rightButton) override {
        const ULONGLONG now = GetTickCount64();
        std::scoped_lock lock(m_mutex);
        (rightButton ? m_right : m_left).push_back(now);
    }

    void buildLines(std::vector<Line>& out) override {
        const bool showRight = settingBool("right", true);

        std::size_t left = 0;
        std::size_t right = 0;
        {
            const ULONGLONG now = GetTickCount64();
            std::scoped_lock lock(m_mutex);
            prune(m_left, now);
            prune(m_right, now);
            left  = m_left.size();
            right = m_right.size();
        }

        push(out, std::to_string(left) + " CPS");
        // Right clicks get their own line rather than a " | " separator. The
        // pipe was doing the job a line break does better, and it made the
        // widget wide enough to collide with anything beside it.
        if (showRight) {
            pushSecondary(out, std::to_string(right) + " RCPS");
        }
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
