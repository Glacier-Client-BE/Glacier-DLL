#include "FullBright.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

FullBright::FullBright()
    : ModuleBase("Full Bright","Maximum ambient lighting",ICON_FA_SUN,ModuleCategory::Visual)
{
    m_settings.defineFloat("brightness","Brightness",  1000.f,100.f,1000.f);
    m_settings.defineBool ("nightVision","Night Vision Tint",false);
}

void FullBright::onEnable() {
    auto* opts = getOptions();
    if (!opts) return;
    auto& orig = OriginalValues::get();
    if (!orig.gammaSaved) {
        orig.gamma      = opts->getGamma();
        orig.gammaSaved = true;
    }
    opts->setGamma(m_settings.getFloat("brightness"));
}

void FullBright::onDisable() {
    auto* opts = getOptions();
    if (!opts) return;
    auto& orig = OriginalValues::get();
    if (orig.gammaSaved) {
        opts->setGamma(orig.gamma);
        orig.gammaSaved = false;
    }
}

void FullBright::onTick() {
    // Re-apply every tick so the game can't revert it
    if (!isEnabled()) return;
    auto* opts = getOptions();
    if (opts) opts->setGamma(m_settings.getFloat("brightness"));
}
