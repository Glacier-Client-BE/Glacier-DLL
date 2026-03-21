#include "HitColor.h"
#include "../../sdk/ClientInstance.h"
#include <imgui.h>
#include <cmath>

HitColor::HitColor()
    : ModuleBase("Hit Color", "Flashes the screen when you hit an entity or take damage",
                 "hitcolor", ModuleCategory::Combat)
{
    m_settings.defineFloat("r",         "Flash R",          255.f, 0.f, 255.f);
    m_settings.defineFloat("g",         "Flash G",            0.f, 0.f, 255.f);
    m_settings.defineFloat("b",         "Flash B",            0.f, 0.f, 255.f);
    m_settings.defineFloat("maxAlpha",  "Max Alpha",          0.18f, 0.01f, 0.5f);
    m_settings.defineFloat("fadeMs",    "Fade (ms)",         200.f, 50.f, 800.f);
    m_settings.defineBool ("onHit",     "On My Attack",       true);
    m_settings.defineBool ("onDamage",  "On Taking Damage",   false);
    m_settings.defineFloat("dmgR",      "Damage R",          255.f, 0.f, 255.f);
    m_settings.defineFloat("dmgG",      "Damage G",            0.f, 0.f, 255.f);
    m_settings.defineFloat("dmgB",      "Damage B",           80.f, 0.f, 255.f);
    m_settings.defineFloat("dmgAlpha",  "Damage Alpha",        0.22f, 0.01f, 0.5f);
}

void HitColor::onEnable()  { m_hitFlash = false; m_dmgFlash = false; m_prevLMB = false; }
void HitColor::onDisable() { m_hitFlash = false; m_dmgFlash = false; }

void HitColor::onRender(ImDrawList* dl) {
    auto now = std::chrono::high_resolution_clock::now();

    // Attack flash — fires on LMB press
    if (m_settings.getBool("onHit")) {
        bool cur = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (cur && !m_prevLMB) {
            m_hitFlash   = true;
            m_lastHitTime = now;
        }
        m_prevLMB = cur;
    }

    // Damage flash — fires when player hurtTime transitions from 0→positive
    if (m_settings.getBool("onDamage")) {
        auto* lp = getLocalPlayer();
        if (lp) {
            float ht = lp->getHurtTime();
            if (ht > 0.f && !m_wasDamaged) {
                m_dmgFlash    = true;
                m_lastDmgTime = now;
            }
            m_wasDamaged = (ht > 0.f);
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    float fadeMs = m_settings.getFloat("fadeMs");

    // Draw attack flash
    if (m_hitFlash) {
        float elapsed = std::chrono::duration<float>(now - m_lastHitTime).count() * 1000.f;
        if (elapsed > fadeMs) {
            m_hitFlash = false;
        } else {
            float t = 1.f - elapsed / fadeMs;
            float a = m_settings.getFloat("maxAlpha") * t;
            dl->AddRectFilled({0,0}, {io.DisplaySize.x, io.DisplaySize.y},
                IM_COL32((int)m_settings.getFloat("r"),
                         (int)m_settings.getFloat("g"),
                         (int)m_settings.getFloat("b"),
                         (int)(a * 255.f)));
        }
    }

    // Draw damage flash (different color, drawn on top)
    if (m_dmgFlash) {
        float elapsed = std::chrono::duration<float>(now - m_lastDmgTime).count() * 1000.f;
        if (elapsed > fadeMs) {
            m_dmgFlash = false;
        } else {
            float t = 1.f - elapsed / fadeMs;
            float a = m_settings.getFloat("dmgAlpha") * t;
            dl->AddRectFilled({0,0}, {io.DisplaySize.x, io.DisplaySize.y},
                IM_COL32((int)m_settings.getFloat("dmgR"),
                         (int)m_settings.getFloat("dmgG"),
                         (int)m_settings.getFloat("dmgB"),
                         (int)(a * 255.f)));
        }
    }
}
