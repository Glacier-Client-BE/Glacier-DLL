#include "Zoom.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <cmath>
#include <algorithm>
#include <imgui.h>

Zoom::Zoom()
    : ModuleBase("Zoom", "Hold a key to zoom in — scroll wheel adjusts level, cinematic mode softens camera",
                 "zoom", ModuleCategory::Utility)
{
    m_settings.defineInt  ("holdKey",         "Hold Key (VK)",       'C',    0x08, 0xFE);
    m_settings.defineFloat("fovMultiplier",   "FOV Multiplier",       0.15f, 0.05f, 0.9f);
    m_settings.defineBool ("smooth",          "Smooth Zoom",          true);
    m_settings.defineFloat("smoothSpeed",     "Smooth Speed",         10.f,  1.f,  30.f);
    m_settings.defineFloat("scrollSens",      "Scroll Sensitivity",   1.f,   0.f,   3.f);
    m_settings.defineBool ("cinematic",       "Cinematic Mode",        false);
    m_settings.defineFloat("cinematicSens",   "Cinematic Sensitivity", 0.3f,  0.05f, 1.f);
    m_settings.defineBool ("resetOnRelease",  "Reset Level on Release",true);
    m_currentFovMult = 1.f;
}

void Zoom::onDisable() {
    auto* opts = getOptions();
    auto& orig = OriginalValues::get();
    if (opts && orig.fovSaved) {
        opts->setFOV(orig.fov);
        orig.fovSaved = false;
    }
    m_currentFovMult = 1.f;
    m_wasHeld = false;
}

void Zoom::onTick() {
    int   holdKey = m_settings.getInt("holdKey");
    bool  held    = (GetAsyncKeyState(holdKey) & 0x8000) != 0;
    auto* opts    = getOptions();
    if (!opts) return;

    auto& orig = OriginalValues::get();

    if (held) {
        if (!orig.fovSaved) {
            orig.fov      = opts->getFOV();
            orig.fovSaved = true;
        }

        float targetMult = m_settings.getFloat("fovMultiplier");

        // Scroll wheel adjusts zoom
        float scroll = ImGui::GetIO().MouseWheel;
        if (fabsf(scroll) > 0.01f) {
            float sens = m_settings.getFloat("scrollSens");
            targetMult = std::clamp(targetMult - scroll * 0.02f * sens, 0.05f, 0.9f);
            m_settings.set("fovMultiplier", targetMult);
        }

        if (m_settings.getBool("smooth")) {
            float speed = m_settings.getFloat("smoothSpeed") * ImGui::GetIO().DeltaTime;
            m_currentFovMult += (targetMult - m_currentFovMult) * std::min(speed, 1.f);
        } else {
            m_currentFovMult = targetMult;
        }

        opts->setFOV(orig.fov * m_currentFovMult);
        m_wasHeld = true;

    } else if (m_wasHeld) {
        // Zoom out
        if (m_settings.getBool("smooth")) {
            float speed = m_settings.getFloat("smoothSpeed") * ImGui::GetIO().DeltaTime;
            m_currentFovMult += (1.f - m_currentFovMult) * std::min(speed, 1.f);
            opts->setFOV(orig.fov * m_currentFovMult);
            if (fabsf(m_currentFovMult - 1.f) < 0.002f) {
                opts->setFOV(orig.fov);
                orig.fovSaved    = false;
                m_currentFovMult = 1.f;
                m_wasHeld        = false;
                if (m_settings.getBool("resetOnRelease"))
                    m_settings.set("fovMultiplier", 0.15f);
            }
        } else {
            opts->setFOV(orig.fov);
            orig.fovSaved    = false;
            m_currentFovMult = 1.f;
            m_wasHeld        = false;
            if (m_settings.getBool("resetOnRelease"))
                m_settings.set("fovMultiplier", 0.15f);
        }
    }
}
