#include "HurtCamDisable.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

HurtCamDisable::HurtCamDisable()
    : ModuleBase("No Hurt Cam","Disables screen shake when taking damage",ICON_FA_VIDEO_SLASH,ModuleCategory::Visual)
{
    m_settings.defineFloat("intensity","Reduce Amount %",100.f,0.f,100.f);
}

void HurtCamDisable::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp) return;
    float pct = m_settings.getFloat("intensity") / 100.f;
    float ht  = lp->getHurtTime();
    if (ht > 0.f) {
        // Scale down hurtTime by the reduction percentage
        // intensity=100 → zero it out entirely
        // intensity=50  → halve it
        lp->setHurtTime(ht * (1.f - pct));
    }
}
