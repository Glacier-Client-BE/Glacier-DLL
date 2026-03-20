#include "FullBright.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>

FullBright::FullBright()
    : ModuleBase("Full Bright", "Maximum ambient lighting — overrides the game's gamma every tick",
                 "fullbright", ModuleCategory::Visual)
{
    m_settings.defineFloat("gamma",       "Gamma Value",       5.f,   1.f,  20.f);
    m_settings.defineBool ("nightVision", "Night Vision Tint", false);
    m_settings.defineBool ("smoothRestore","Smooth Restore",   true);
}

void FullBright::onEnable() {
    auto* opts = getOptions();
    if (!opts) return;
    auto& orig = OriginalValues::get();
    if (!orig.gammaSaved) {
        orig.gamma      = opts->getGamma();
        orig.gammaSaved = true;
    }
    opts->setGamma(m_settings.getFloat("gamma"));
}

void FullBright::onDisable() {
    auto* opts = getOptions();
    if (!opts) return;
    auto& orig = OriginalValues::get();
    if (orig.gammaSaved) {
        opts->setGamma(orig.gamma);
        orig.gammaSaved = false;
        m_restoreTarget = orig.gamma;
        m_restoring     = m_settings.getBool("smoothRestore");
    }
}

void FullBright::onTick() {
    auto* opts = getOptions();
    if (!opts) return;

    if (m_restoring) {
        float cur = opts->getGamma();
        float delta = (m_restoreTarget - cur) * 0.15f;
        if (fabsf(delta) < 0.01f) {
            opts->setGamma(m_restoreTarget);
            m_restoring = false;
        } else {
            opts->setGamma(cur + delta);
        }
        return;
    }

    if (!isEnabled()) return;
    // Re-apply every tick so the game can't revert it
    opts->setGamma(m_settings.getFloat("gamma"));
}
