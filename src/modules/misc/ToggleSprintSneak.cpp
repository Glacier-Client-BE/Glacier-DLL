#include "ToggleSprintSneak.h"

namespace Glacier::modules {

ToggleSprintSneak::ToggleSprintSneak()
    : Module("toggle_sprint_sneak", "Toggle Sprint/Sneak", "Press once to latch the input", Category::Movement),
      toggleSprint_(addSetting<BoolSetting>("toggle_sprint", "Toggle Sprint", "Latch sprint when held once", true)),
      toggleSneak_ (addSetting<BoolSetting>("toggle_sneak",  "Toggle Sneak",  "Latch sneak when held once",  true))
{}

} // namespace Glacier::modules
