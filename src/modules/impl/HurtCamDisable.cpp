#include "HurtCamDisable.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

HurtCamDisable::HurtCamDisable()
    : ModuleBase("No Hurt Cam", "Suppresses camera shake on damage and optional nausea effect",
                 "hurtcamdisable", ModuleCategory::Visual)
{
    m_settings.defineFloat("intensity",      "Hurt Reduce %",        100.f, 0.f, 100.f);
    m_settings.defineBool ("suppressNausea", "Suppress Nausea Cam",   true);
    m_settings.defineBool ("suppressDark",   "Suppress Darkness Cam", false);
}

void HurtCamDisable::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp) return;

    float pct = m_settings.getFloat("intensity") / 100.f;

    // Reduce hurt camera shake
    float ht = lp->getHurtTime();
    if (ht > 0.f)
        lp->setHurtTime(ht * (1.f - pct));

    // Suppress nausea: clear the nausea effect if the SDK supports it
    if (m_settings.getBool("suppressNausea"))
        lp->removeEffect(EffectId::Nausea);

    // Suppress darkness (from Warden/Deep Dark)
    if (m_settings.getBool("suppressDark"))
        lp->removeEffect(EffectId::Darkness);
}
