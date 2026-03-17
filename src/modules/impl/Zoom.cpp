#include "Zoom.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <cmath>

Zoom::Zoom()
    : ModuleBase("Zoom","Hold C to zoom the camera",ICON_FA_SEARCH_PLUS,ModuleCategory::Utility)
{
    m_settings.defineFloat("fovMultiplier",   "FOV Multiplier",  0.15f, 0.05f, 0.5f);
    m_settings.defineBool ("smooth",          "Smooth Zoom",     true);
    m_settings.defineFloat("smoothSpeed",     "Smooth Speed",    8.f,   1.f,   20.f);
    m_settings.defineFloat("scrollSensitivity","Scroll Adjust",  1.f,   0.f,   3.f);
    m_settings.defineBool ("cinematic",       "Cinematic Mode",  false);
    m_settings.defineFloat("cinematicSpeed",  "Cinematic Sens",  0.3f,  0.05f, 1.f);
    m_currentFovMult = 1.f;
}

void Zoom::onDisable() {
    // Restore FOV immediately
    auto* opts = getOptions();
    auto& orig = OriginalValues::get();
    if (opts && orig.fovSaved) {
        opts->setFOV(orig.fov);
        orig.fovSaved = false;
    }
    m_currentFovMult = 1.f;
}

void Zoom::onTick() {
    bool held = (GetAsyncKeyState('C') & 0x8000) != 0;
    auto* opts = getOptions();
    if (!opts) return;

    auto& orig = OriginalValues::get();

    if (held) {
        // Save original FOV once
        if (!orig.fovSaved) {
            orig.fov      = opts->getFOV();
            orig.fovSaved = true;
        }

        float targetMult = m_settings.getFloat("fovMultiplier");

        // Allow scroll wheel to adjust zoom level
        // (ImGui IO delta is checked — works within the overlay frame)
        float scrollDelta = ImGui::GetIO().MouseWheel;
        if (fabsf(scrollDelta) > 0.01f) {
            float sens = m_settings.getFloat("scrollSensitivity");
            targetMult -= scrollDelta * 0.02f * sens;
            if (targetMult < 0.05f) targetMult = 0.05f;
            if (targetMult > 0.9f ) targetMult = 0.9f;
            m_settings.set("fovMultiplier", targetMult);
        }

        if (m_settings.getBool("smooth")) {
            float speed = m_settings.getFloat("smoothSpeed") * ImGui::GetIO().DeltaTime;
            m_currentFovMult += (targetMult - m_currentFovMult) * speed;
        } else {
            m_currentFovMult = targetMult;
        }

        opts->setFOV(orig.fov * m_currentFovMult);

    } else {
        // Smooth zoom-out
        if (orig.fovSaved) {
            if (m_settings.getBool("smooth")) {
                float speed = m_settings.getFloat("smoothSpeed") * ImGui::GetIO().DeltaTime;
                m_currentFovMult += (1.f - m_currentFovMult) * speed;
                opts->setFOV(orig.fov * m_currentFovMult);
                if (fabsf(m_currentFovMult - 1.f) < 0.001f) {
                    opts->setFOV(orig.fov);
                    orig.fovSaved    = false;
                    m_currentFovMult = 1.f;
                }
            } else {
                opts->setFOV(orig.fov);
                orig.fovSaved    = false;
                m_currentFovMult = 1.f;
            }
        }
    }
}
