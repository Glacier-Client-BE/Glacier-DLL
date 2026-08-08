#pragma once

#include <atomic>
#include <Windows.h>

// Frame timing, sampled once per Present and read by any widget that wants it.
//
// Kept out of the individual HUD modules so two widgets showing FPS can't
// disagree: a per-module counter only advances while that module is enabled, so
// two of them would report different numbers, and a freshly enabled one would
// read zero for a second.
namespace glacier {

class FrameStats {
public:
    static FrameStats& get() {
        static FrameStats instance;
        return instance;
    }

    // Called once per frame from the Present hook, before anything draws.
    void onFrame() {
        ++m_frames;
        const ULONGLONG now = GetTickCount64();
        const ULONGLONG span = now - m_lastSample;
        if (span >= 500) {
            m_fps.store(static_cast<int>(m_frames * 1000ULL / span),
                        std::memory_order_relaxed);
            m_frames = 0;
            m_lastSample = now;
        }
    }

    int fps() const { return m_fps.load(std::memory_order_relaxed); }

    // Milliseconds per frame, derived from the same fps() rather than tracked
    // separately — two readouts computed from independent samples could
    // disagree by a frame or two, which is exactly the "FPS Counter and Frame
    // Time Display don't quite match" bug a second counter would invite.
    float frameTimeMs() const {
        const int f = fps();
        return f > 0 ? 1000.0f / static_cast<float>(f) : 0.0f;
    }

    // Seconds since the client attached — for a session timer.
    int sessionSeconds() const {
        return static_cast<int>((GetTickCount64() - m_start) / 1000);
    }

private:
    FrameStats() = default;

    ULONGLONG        m_start = GetTickCount64();
    ULONGLONG        m_lastSample = GetTickCount64();
    unsigned         m_frames = 0;
    std::atomic<int> m_fps{ 0 };
};

} // namespace glacier
