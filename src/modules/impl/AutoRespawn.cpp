#include "AutoRespawn.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <chrono>

AutoRespawn::AutoRespawn()
    : ModuleBase("Auto Respawn", "Automatically clicks Respawn after death with configurable delay",
                 "autorespawn", ModuleCategory::Utility)
{
    m_settings.defineFloat("delay",    "Respawn Delay (s)", 0.25f, 0.f, 5.f);
    m_settings.defineBool ("notify",   "Notify on Respawn", true);
    m_settings.defineBool ("onlyPVP",  "Only in Multiplayer", false);
}

void AutoRespawn::onEnable()  { m_deathTime = {}; m_waiting = false; }
void AutoRespawn::onDisable() { m_waiting = false; }

void AutoRespawn::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp) return;

    // Only-multiplayer check
    if (m_settings.getBool("onlyPVP") && lp->getNetworkPing() <= 0) return;

    bool dead = lp->isDead() || !lp->isAlive();

    if (dead && !m_waiting) {
        m_deathTime = std::chrono::high_resolution_clock::now();
        m_waiting   = true;
    }

    if (m_waiting) {
        if (!dead) {
            m_waiting = false;
            return;
        }
        float elapsed = std::chrono::duration<float>(
            std::chrono::high_resolution_clock::now() - m_deathTime).count();
        if (elapsed >= m_settings.getFloat("delay")) {
            auto* lvl = getLevel();
            if (lvl) lvl->sendRespawnPacket();
            m_waiting = false;
        }
    }
}
