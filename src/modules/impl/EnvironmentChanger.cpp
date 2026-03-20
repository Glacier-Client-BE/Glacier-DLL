#include "EnvironmentChanger.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <ctime>

EnvironmentChanger::EnvironmentChanger()
    : ModuleBase("Env Changer", "Override time-of-day, fog color, and weather visuals",
                 "environmentchanger", ModuleCategory::Visual)
{
    m_settings.defineBool ("changeTime",  "Override Time",       true);
    m_settings.defineBool ("useRealTime", "Use Real-World Time", false);
    m_settings.defineFloat("time",        "Time (0=midnight, 0.5=noon)", 0.5f, 0.f, 1.f);
    m_settings.defineBool ("changeGamma", "Override Gamma",      false);
    m_settings.defineFloat("gamma",       "Gamma",               2.0f, 0.f, 5.f);
    m_settings.defineBool ("changeRain",  "Override Rain",       false);
    m_settings.defineFloat("rainLevel",   "Rain Intensity",      0.f,  0.f, 1.f);
    m_settings.defineBool ("noFog",       "Reduce Fog",          false);
}

void EnvironmentChanger::onEnable() {
    auto* opts = getOptions();
    if (opts) m_savedGamma = opts->getGamma();
}

void EnvironmentChanger::onDisable() {
    auto* opts = getOptions();
    if (opts) opts->setGamma(m_savedGamma);
}

void EnvironmentChanger::onTick() {
    auto* opts = getOptions();
    if (!opts) return;

    if (m_settings.getBool("changeGamma"))
        opts->setGamma(m_settings.getFloat("gamma"));

    // Time override is handled by engine hooks (stub: gamma is the only hookable field here)
}
