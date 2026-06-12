#pragma once

#include <algorithm>
#include <vector>
#include <Windows.h>

#include "../HudModule.h"

namespace glacier {

// Tracks clicks-per-second from the WndProc mouse fan-out (handleClick ->
// onClick) and exposes it as a corner-docked text readout.
class CpsCounter final : public HudModule {
public:
    CpsCounter()
        : HudModule("CPS Counter", "Displays your clicks per second", Category::Misc,
                    /*positioned*/ false, "top-right") {
        addSetting(Setting{ "rightClick", "Count right clicks", false });
    }

    void onClick(bool rightButton) override {
        const auto* rc = setting("rightClick");
        if (rightButton && !(rc && rc->asBool())) return;   // right clicks opt-in
        m_clicks.push_back(now());
        prune();
    }

    int cps() const {
        const double cutoff = now() - 1000.0;
        return static_cast<int>(std::count_if(
            m_clicks.begin(), m_clicks.end(),
            [cutoff](double t) { return t >= cutoff; }));
    }

protected:
    void writeHudBody(json::Object& o) const override {
        o.set("type", "text").set("id", "cps").set("anchor", anchor())
         .set("label", "CPS").set("value", std::to_string(cps()));
    }

private:
    static double now() { return static_cast<double>(GetTickCount64()); }
    void prune() {
        const double cutoff = now() - 1000.0;
        std::erase_if(m_clicks, [cutoff](double t) { return t < cutoff; });
    }

    std::vector<double> m_clicks;
};

} // namespace glacier
