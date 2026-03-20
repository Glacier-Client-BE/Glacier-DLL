#include "ToggleSneak.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

ToggleSneak::ToggleSneak()
    : ModuleBase("Toggle Sneak", "Sneak without holding the sneak key",
                 "togglesneak", ModuleCategory::Movement)
{
    m_settings.defineBool("persistent", "Keep After Disable", false);
}

void ToggleSneak::onEnable() {
    auto* lp = getLocalPlayer();
    if (lp) lp->setSneaking(true);
    m_sneakToggled = true;
}

void ToggleSneak::onDisable() {
    if (!m_settings.getBool("persistent")) {
        auto* lp = getLocalPlayer();
        if (lp) lp->setSneaking(false);
    }
    m_sneakToggled = false;
}

void ToggleSneak::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp) return;
    lp->setSneaking(true);
}
