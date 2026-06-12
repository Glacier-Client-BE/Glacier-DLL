#pragma once

#include "../HudModule.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Free-placement paper-doll: a small player-model panel that can sit anywhere
// on screen. Glacier renders a stylized doll in the web layer; this module
// supplies position/scale plus a live "facing" angle derived from movement so
// the doll turns as you move. (A full skin-rendered doll needs the actor
// renderer; this is the lightweight HUD-overlay form.)
class MovablePaperdoll final : public HudModule {
public:
    MovablePaperdoll()
        : HudModule("Movable Paperdoll", "Freely repositionable paper-doll panel",
                    Category::Visual, /*positioned*/ true, "", 24, 320) {
        addSetting(Setting{ "sneaking", "Show pose changes", true });
    }

protected:
    void writeHudBody(json::Object& o) const override {
        o.set("type", "paperdoll").set("id", "paperdoll")
         .set("valid", sdk::GameSDK::get().playerPosition().has_value());
    }
};

} // namespace glacier
