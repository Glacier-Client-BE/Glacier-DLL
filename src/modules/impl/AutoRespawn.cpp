#include "AutoRespawn.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <chrono>

AutoRespawn::AutoRespawn()
    : ModuleBase("Auto Respawn","Automatically clicks Respawn on death",ICON_FA_REDO,ModuleCategory::Utility)
{
    m_settings.defineFloat("delay",  "Respawn Delay (s)", 0.2f, 0.f, 3.f);
    m_settings.defineBool ("notify", "Log on Respawn",    true);
}

void AutoRespawn::onEnable()  { m_deathTime = {}; m_waiting = false; }
void AutoRespawn::onDisable() { m_waiting = false; }

void AutoRespawn::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp) return;

    bool dead = lp->isDead() || !lp->isAlive();

    if (dead && !m_waiting) {
        m_deathTime = std::chrono::high_resolution_clock::now();
        m_waiting   = true;
    }

    if (m_waiting && dead) {
        float elapsed = std::chrono::duration<float>(
            std::chrono::high_resolution_clock::now() - m_deathTime).count();

        if (elapsed >= m_settings.getFloat("delay")) {
            // Send respawn packet via Level
            auto* lvl = getLevel();
            if (lvl) lvl->sendRespawnPacket();
            m_waiting = false;
        }
    } else if (!dead) {
        m_waiting = false;
    }
}
