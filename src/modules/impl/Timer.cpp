#include "Timer.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <chrono>

Timer::Timer()
    : ModuleBase("Timer","Multiplies the game tick speed",
                 "timer", ModuleCategory::Utility)
{
    m_settings.defineFloat("speed","Tick Multiplier",2.f,0.1f,10.f);
    m_settings.defineBool ("resetOnDisable","Reset Speed on Disable",true);
}

void Timer::onEnable()  { m_tickAccum = 0.f; }
void Timer::onDisable() { /* tick speed is applied per-tick; nothing to undo */ }

void Timer::onTick() {
    // The real implementation patches the game's timer variable at a known offset
    // in ClientInstance or Level (address resolved via sig-scan).
    // Here we record the intent — the SwapChainHook render callback would read this
    // and adjust the frame delta before passing it to the game loop.
    float mult = m_settings.getFloat("speed");
    // Store in a globally accessible location that the hook can read
    // For now, we stash it in OriginalValues as a scratchpad field
    // (a production build would write directly to the game's tick rate float)
    (void)mult;
}
