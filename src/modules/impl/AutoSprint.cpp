#include "AutoSprint.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

// Toggle Sprint: keeps sprint active whenever the player is moving forward.
// Unlike Auto Sprint, it does NOT force-sprint in all directions or without input —
// the player must be pressing W. It simply removes the need to hold the sprint key.

AutoSprint::AutoSprint()
    : ModuleBase("Toggle Sprint","Keeps sprint active while moving forward",ICON_FA_RUNNING,ModuleCategory::Movement)
{
    m_settings.defineBool("noSneak", "Disable While Sneaking", true);
}

void AutoSprint::onEnable()  { m_sprinting = false; }
void AutoSprint::onDisable() {
    auto* lp = getLocalPlayer();
    if (lp) lp->setSprinting(false);
    m_sprinting = false;
}

void AutoSprint::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp || !lp->isAlive()) return;

    // Respect sneak
    if (m_settings.getBool("noSneak") && lp->isSneaking()) {
        lp->setSprinting(false);
        m_sprinting = false;
        return;
    }

    bool movingForward = (GetAsyncKeyState('W') & 0x8000) != 0;

    if (movingForward) {
        // Player is pressing W — keep sprint engaged
        lp->setSprinting(true);
        m_sprinting = true;
    } else {
        // Not pressing W — let sprint fall off naturally
        m_sprinting = false;
    }
}
