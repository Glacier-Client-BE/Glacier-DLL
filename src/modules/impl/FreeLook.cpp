#include "FreeLook.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <Windows.h>

FreeLook::FreeLook()
    : ModuleBase("Free Look", "Hold a key to orbit the camera without turning your character",
                 "freelook", ModuleCategory::Visual)
{
    m_settings.defineInt ("holdKey",      "Hold Key (VK)",  18,    0x08, 0xFE); // ALT
    m_settings.defineBool("smoothReturn", "Smooth Return",  true);
    m_settings.defineFloat("sensitivity", "Sensitivity",    1.0f,  0.1f, 3.f);
}

void FreeLook::onEnable()  { m_active = false; }
void FreeLook::onDisable() {
    auto* lp = getLocalPlayer();
    if (lp && m_active) {
        lp->setYaw(m_savedYaw);
        lp->setPitch(m_savedPitch);
    }
    m_active = false;
}

void FreeLook::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp) return;

    int holdKey = m_settings.getInt("holdKey");
    bool holding = (GetAsyncKeyState(holdKey) & 0x8000) != 0;

    if (holding && !m_active) {
        m_savedYaw   = lp->getYaw();
        m_savedPitch = lp->getPitch();
        m_active     = true;
    }

    if (!holding && m_active) {
        if (m_settings.getBool("smoothReturn")) {
            float cy = lp->getYaw(), ty = m_savedYaw;
            float cp = lp->getPitch(), tp = m_savedPitch;
            lp->setYaw(cy + (ty - cy) * 0.2f);
            lp->setPitch(cp + (tp - cp) * 0.2f);
            if (fabsf(cy - ty) < 0.5f && fabsf(cp - tp) < 0.5f) m_active = false;
        } else {
            lp->setYaw(m_savedYaw);
            lp->setPitch(m_savedPitch);
            m_active = false;
        }
    }
}
