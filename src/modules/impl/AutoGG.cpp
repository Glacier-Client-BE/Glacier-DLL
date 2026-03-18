#include "AutoGG.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

AutoGG::AutoGG()
    : ModuleBase("Auto GG", "Automatically sends a GG message at the end of a game",
                 ICON_FA_TROPHY, ModuleCategory::Utility)
{
    m_settings.defineString("message",  "GG Message", "gg");
    m_settings.defineString("glMessage","GL Message",  "gl hf");
    m_settings.defineBool  ("sendGL",   "Send GL at start", true);
    m_settings.defineBool  ("addPrefix","Add Prefix (/gc)", false);
    m_settings.defineInt   ("delay",    "Delay (ticks)", 20, 0, 120);
}

void AutoGG::onEnable()  { m_sentGG = false; m_sentGL = false; m_cooldown = 0; }
void AutoGG::onDisable() { m_sentGG = false; m_sentGL = false; m_cooldown = 0; }

void AutoGG::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp || !lp->isAlive()) return;

    if (m_cooldown > 0) { --m_cooldown; return; }

    auto* lvl = getLevel();
    if (!lvl) return;

    if (!m_sentGL && m_settings.getBool("sendGL")) {
        std::string msg = m_settings.getString("glMessage");
        if (m_settings.getBool("addPrefix")) msg = "/gc " + msg;
        m_sentGL    = true;
        m_cooldown  = m_settings.getInt("delay");
    }
}
