#include "FreeCam.h"
#include "../../sdk/ClientInstance.h"
#include "../../utils/Logger.h"
#include <IconsFontAwesome5.h>
#include <cmath>

FreeCam::FreeCam()
    : ModuleBase("Free Cam", "Detaches camera from player (singleplayer only)", ICON_FA_VIDEO, ModuleCategory::Visual)
{
    m_settings.defineFloat("speed",  "Camera Speed",     1.f, 0.1f, 5.f);
    m_settings.defineBool ("revert", "Revert on Disable", true);
    m_settings.defineBool ("noClip", "No Clip",           true);
}

void FreeCam::onEnable() {
    auto* lp = getLocalPlayer();
    if (!lp) return;

    // Check if on a server using getNetworkPing() from your SDK
    // In local worlds, ping is typically 0 or negative.
    if (lp->getNetworkPing() > 0) { 
        Logger::warn("Free Cam is disabled on multiplayer servers.");
        setEnabled(false);
        return;
    }

    auto& fc = FreeCamState::get();
    fc.savedPos   = lp->getPosition();
    fc.cameraPos  = fc.savedPos;
    fc.savedYaw   = lp->getYaw();
    fc.savedPitch = lp->getPitch();
    fc.active     = true;
}

void FreeCam::onDisable() {
    auto& fc = FreeCamState::get();
    fc.active = false;
    
    if (m_settings.getBool("revert")) {
        auto* lp = getLocalPlayer();
        if (lp) {
            lp->setYaw(fc.savedYaw);
            lp->setPitch(fc.savedPitch);
        }
    }
}

void FreeCam::onTick() {
    auto* lp = getLocalPlayer();
    if (!lp) return;

    // Continuous safety check for server connection
    if (lp->getNetworkPing() > 0) {
        setEnabled(false);
        return;
    }

    float speed = m_settings.getFloat("speed") * 0.2f;
    auto& fc    = FreeCamState::get();

    float yaw    = lp->getYaw();
    float pitch  = lp->getPitch();
    float yawRad = yaw * 3.14159265f / 180.f;

    float dx = 0.f, dy = 0.f, dz = 0.f;

    if (GetAsyncKeyState('W') & 0x8000) {
        dx -= sinf(yawRad) * speed;
        dz += cosf(yawRad) * speed;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        dx += sinf(yawRad) * speed;
        dz -= cosf(yawRad) * speed;
    }
    if (GetAsyncKeyState('A') & 0x8000) {
        dx -= cosf(yawRad) * speed;
        dz -= sinf(yawRad) * speed;
    }
    if (GetAsyncKeyState('D') & 0x8000) {
        dx += cosf(yawRad) * speed;
        dz += sinf(yawRad) * speed;
    }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) dy += speed;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) dy -= speed;

    fc.cameraPos.x += dx;
    fc.cameraPos.y += dy;
    fc.cameraPos.z += dz;

    // Freeze real player in place
    // Offsets 0x110, 0x114, 0x118 match your Actor::getVelocity() layout
    float* vx = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(lp) + 0x110);
    float* vy = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(lp) + 0x114);
    float* vz = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(lp) + 0x118);
    
    if (vx && vy && vz) {
        *vx = 0.f; 
        *vy = 0.f; 
        *vz = 0.f;
    }

    (void)pitch;
}