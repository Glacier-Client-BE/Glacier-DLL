#pragma once

#include <format>

#include "../HudModule.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Free-placement coordinate readout. Unlike the corner-docked Coordinates
// module, this is a positioned HUD widget (drag-anywhere x/y + scale), for
// players who want XYZ exactly where the native MC coordinate text used to sit.
class MovableCoordinates final : public HudModule {
public:
    MovableCoordinates()
        : HudModule("Movable Coordinates", "Freely repositionable XYZ display",
                    Category::Misc, /*positioned*/ true, "", 8, 60) {
        addSetting(Setting{ "dimensionScale", "Show Nether (÷8)", false });
    }

    void onRender() override {
        if (auto p = sdk::GameSDK::get().playerPosition()) {
            m_pos = *p;
            m_valid = true;
        } else {
            m_valid = false;
        }
    }

protected:
    void writeHudBody(json::Object& o) const override {
        o.set("type", "panel").set("id", "movecoords").set("label", "XYZ")
         .set("value", m_valid
                  ? std::format("{:.0f}, {:.0f}, {:.0f}", m_pos.x, m_pos.y, m_pos.z)
                  : "—");
        if (m_valid) {
            const auto* nether = setting("dimensionScale");
            if (nether && nether->asBool()) {
                o.set("sub", std::format("Nether {:.0f}, {:.0f}", m_pos.x / 8.0f, m_pos.z / 8.0f));
            }
        }
    }

private:
    sdk::Vec3 m_pos{};
    bool      m_valid = false;
};

} // namespace glacier
