#include "ModuleManager.h"
// ── HUD ───────────────────────────────────────────────────────────────────────
#include "impl/FPSCounter.h"
#include "impl/Keystrokes.h"
#include "impl/CPSCounter.h"
#include "impl/ArmorHUD.h"
#include "impl/CoordinatesHUD.h"
#include "impl/SpeedHUD.h"
#include "impl/ActiveModsList.h"
#include "impl/ClockCompass.h"
#include "impl/PingDisplay.h"
#include "impl/ReachDisplay.h"
#include "impl/ClickStatsDashboard.h"
#include "impl/InventoryViewer.h"
#include "impl/SessionInfo.h"
#include "impl/PotionHUD.h"
#include "impl/ChunkMap.h"
// ── Combat ────────────────────────────────────────────────────────────────────
#include "impl/ComboCounter.h"
#include "impl/TargetHUD.h"
#include "impl/KillCounter.h"
#include "impl/HitColor.h"
// ── Movement ──────────────────────────────────────────────────────────────────
#include "impl/SafeWalk.h"
#include "impl/ToggleSneak.h"
// ── Visual ────────────────────────────────────────────────────────────────────
#include "impl/FullBright.h"
#include "impl/HurtCamDisable.h"
#include "impl/FreeCam.h"
#include "impl/ChunkBorders.h"
#include "impl/Crosshair.h"
#include "impl/HealthIndicator.h"
#include "impl/BlockOutline.h"
#include "impl/Hitboxes.h"
#include "impl/FreeLook.h"
#include "impl/EnvironmentChanger.h"
// ── Utility ───────────────────────────────────────────────────────────────────
#include "impl/Zoom.h"
#include "impl/Timer.h"
#include "impl/NotificationSystem.h"
#include "impl/PacketLogger.h"
#include "impl/AutoGG.h"
#include "impl/NickHider.h"
#include "impl/Waypoints.h"
#include "../utils/Logger.h"

ModuleManager& ModuleManager::get() { static ModuleManager i; return i; }

void ModuleManager::init() {
    // ── HUD ───────────────────────────────────────────────────────────────
    addModule<FPSCounter>();
    addModule<Keystrokes>();
    addModule<CPSCounter>();
    addModule<ArmorHUD>();
    addModule<CoordinatesHUD>();
    addModule<SpeedHUD>();
    addModule<ActiveModsList>();
    addModule<ClockCompass>();
    addModule<PingDisplay>();
    addModule<ReachDisplay>();
    addModule<ClickStatsDashboard>();
    addModule<InventoryViewer>();
    addModule<SessionInfo>();
    addModule<PotionHUD>();
    addModule<ChunkMap>();

    // ── Combat ────────────────────────────────────────────────────────────
    addModule<ComboCounter>();
    addModule<TargetHUD>();
    addModule<KillCounter>();
    addModule<HitColor>();

    // ── Movement ──────────────────────────────────────────────────────────
    addModule<SafeWalk>();
    addModule<ToggleSneak>();

    // ── Visual ────────────────────────────────────────────────────────────
    addModule<FullBright>();
    addModule<HurtCamDisable>();
    addModule<FreeCam>();
    addModule<ChunkBorders>();
    addModule<Crosshair>();
    addModule<HealthIndicator>();
    addModule<BlockOutline>();
    addModule<Hitboxes>();
    addModule<FreeLook>();
    addModule<EnvironmentChanger>();

    // ── Utility ───────────────────────────────────────────────────────────
    addModule<Zoom>();
    addModule<Timer>();
    addModule<NotificationSystem>();
    addModule<PacketLogger>();
    addModule<AutoGG>();
    addModule<NickHider>();
    addModule<Waypoints>();

    Logger::info("ModuleManager: {} modules registered", m_modules.size());
}

void ModuleManager::shutdown() {
    for (auto& m : m_modules) m->setEnabled(false);
    m_modules.clear();
}

void ModuleManager::onTick() {
    for (auto& m : m_modules) {
        if (m->isEnabled()) m->onTick();
        m->pollHotkey();
    }
}

void ModuleManager::onRender(ImDrawList* dl) {
    for (auto& m : m_modules)
        if (m->isEnabled()) m->onRender(dl);
}

void ModuleManager::onRenderImGui() {
    for (auto& m : m_modules)
        if (m->isEnabled()) m->onRenderImGui();
}
