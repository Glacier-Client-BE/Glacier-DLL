#pragma once
#include "../Module.h"

namespace Glacier::modules {

class ToggleSprintSneak final : public Module {
public:
    ToggleSprintSneak();
private:
    BoolSetting& toggleSprint_;
    BoolSetting& toggleSneak_;
};

} // namespace Glacier::modules
