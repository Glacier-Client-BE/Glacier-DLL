#pragma once

#include "../Module.h"

namespace glacier {

// Forces the sprint state on every tick the player is moving forward, so the
// user never has to hold the sprint key. Logic that flips the player's sprint
// flag lives in the move-input hook; the module just gates it.
class AutoSprint final : public Module {
public:
    AutoSprint()
        : Module("AutoSprint", "Always sprint while moving forward", Category::Movement) {
        addSetting(Setting{ "omni", "Sprint sideways/backwards", false });
    }

    bool omnidirectional() const {
        const auto* s = setting("omni");
        return s && s->asBool();
    }
};

} // namespace glacier
