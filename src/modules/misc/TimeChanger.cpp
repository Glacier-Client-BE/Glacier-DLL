#include "TimeChanger.h"

namespace Glacier::modules {

TimeChanger::TimeChanger()
    : Module("time_changer", "Time Changer", "Visual override of in-game time", Category::Misc),
      preset_(addSetting<EnumSetting>("preset", "Preset", "Quick presets",
                  std::vector<std::string>{ "Day", "Sunset", "Night", "Sunrise", "Custom" }, 0)),
      custom_(addSetting<IntSetting> ("custom", "Custom", "0..23999 ticks", 6000, 0, 23999))
{}

} // namespace Glacier::modules
