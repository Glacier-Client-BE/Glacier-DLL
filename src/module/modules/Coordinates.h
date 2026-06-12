#pragma once

#include <format>

#include "../HudModule.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Displays the local player's XYZ. Snapshots position on the render thread (so
// the SDK pointers are live) and renders a corner-docked text readout.
class Coordinates final : public HudModule {
public:
    Coordinates()
        : HudModule("Coordinates", "Displays your XYZ position",
                    Category::World, /*positioned*/ false, "bottom-left") {
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
        std::string value = m_valid
            ? std::format("{:.0f}, {:.0f}, {:.0f}", m_pos.x, m_pos.y, m_pos.z)
            : "—";
        o.set("type", "text").set("id", "coords").set("anchor", anchor())
         .set("label", "XYZ").set("value", value);

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
