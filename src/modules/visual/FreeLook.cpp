#include "FreeLook.h"
#include "../../core/Logger.h"
#include "../../events/EventBus.h"

namespace Glacier::modules {

FreeLook::FreeLook()
    : Module("free_look", "Free Look", "Hold a key to look around without turning your body", Category::Visual),
      holdKey_   (addSetting<KeybindSetting>("hold_key", "Hold Key", "Activation key", 'V')),
      perspective_(addSetting<BoolSetting>  ("perspective","Force 3rd Person","Switch view while held", true))
{
    EventBus::get().listen<KeyEvent, &FreeLook::onKey>(this);
}

void FreeLook::onKey(KeyEvent& k) {
    if (k.consumedByGui) return;
    if (k.vkey != holdKey_.vkey()) return;
    bool was = active_;
    active_  = k.down && enabled_;
    if (was != active_) {
        Logger::get().info("Mod", "FreeLook ", active_ ? "engaged" : "released");
    }
}

} // namespace Glacier::modules
