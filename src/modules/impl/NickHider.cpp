#include "NickHider.h"
#include <IconsFontAwesome5.h>

NickHider::NickHider()
    : ModuleBase("Nick Hider", "Hides your real username in the tab list and above your head",
                 ICON_FA_USER_SECRET, ModuleCategory::Utility)
{
    m_settings.defineString("fakeName", "Display Name", "Steve");
    m_settings.defineBool  ("hideTab",  "Hide in Tab",   true);
    m_settings.defineBool  ("hideHead", "Hide Nametag",  true);
}
