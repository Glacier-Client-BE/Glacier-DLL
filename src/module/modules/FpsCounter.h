#pragma once

#include <Windows.h>

#include "../HudModule.h"

namespace glacier {

// Measures real frame rate from the present loop. onRender() is invoked once per
// IDXGISwapChain::Present (via ModuleManager::renderAll), so we derive FPS from
// the wall-clock delta between calls and smooth it with an exponential moving
// average. Docks to a screen corner as a text readout.
class FpsCounter final : public HudModule {
public:
    FpsCounter()
        : HudModule("FPS Counter", "Displays your current frame rate", Category::Misc,
                    /*positioned*/ false, "top-right") {
        addSetting(Setting{ "smooth", "Smoothing", 0.90f, 0.0f, 0.99f, 0.01f });
        addSetting(Setting{ "showAvg", "Show 1% low", true });
    }

    void onEnable() override {
        m_lastCounter = now();
        m_fps = 0.0;
        m_onePercentLow = 0.0;
    }

    void onRender() override {
        const std::int64_t t = now();
        const double dt = static_cast<double>(t - m_lastCounter) / static_cast<double>(frequency());
        m_lastCounter = t;
        if (dt <= 0.0) return;

        const double instant = 1.0 / dt;

        double alpha = 0.90;
        if (auto* s = setting("smooth")) alpha = s->asFloat();
        m_fps = (m_fps <= 0.0) ? instant : (alpha * m_fps + (1.0 - alpha) * instant);

        if (m_onePercentLow <= 0.0 || instant < m_onePercentLow) {
            m_onePercentLow = instant;
        } else {
            m_onePercentLow += (m_fps - m_onePercentLow) * 0.005;
        }
    }

    int fps() const { return static_cast<int>(m_fps + 0.5); }
    int onePercentLow() const { return static_cast<int>(m_onePercentLow + 0.5); }

protected:
    void writeHudBody(json::Object& o) const override {
        o.set("type", "text").set("id", "fps").set("anchor", anchor())
         .set("label", "FPS").set("value", std::to_string(fps()));
        const auto* showAvg = setting("showAvg");
        if (showAvg && showAvg->asBool()) {
            o.set("sub", "1% low " + std::to_string(onePercentLow()));
        }
    }

private:
    static std::int64_t now() {
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        return li.QuadPart;
    }
    static std::int64_t frequency() {
        static const std::int64_t freq = [] {
            LARGE_INTEGER li;
            QueryPerformanceFrequency(&li);
            return li.QuadPart;
        }();
        return freq;
    }

    std::int64_t m_lastCounter   = 0;
    double       m_fps           = 0.0;
    double       m_onePercentLow = 0.0;
};

} // namespace glacier
