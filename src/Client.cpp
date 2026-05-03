#include "Client.h"
#include "core/Glacier.h"
#include "core/Logger.h"
#include "core/Util.h"
#include "events/EventBus.h"
#include "events/Events.h"
#include "hooks/HookManager.h"
#include "hooks/render/SwapChainHook.h"
#include "hooks/render/WndProcHook.h"
#include "render/Renderer.h"
#include "modules/ModuleManager.h"
#include "config/Config.h"
#include "sdk/Addresses.h"
#include "gui/ClickGui.h"

#include "modules/hud/Watermark.h"
#include "modules/hud/FPSCounter.h"
#include "modules/hud/Coordinates.h"
#include "modules/hud/Keystrokes.h"
#include "modules/hud/CPS.h"
#include "modules/hud/ArmorHUD.h"
#include "modules/hud/ClientBranding.h"
#include "modules/hud/PlayerList.h"

#include "modules/visual/Crosshair.h"
#include "modules/visual/ChunkBorders.h"
#include "modules/visual/BlockOutline.h"
#include "modules/visual/Fullbright.h"
#include "modules/visual/Zoom.h"
#include "modules/visual/FreeLook.h"
#include "modules/visual/NoHurtCam.h"

#include "modules/misc/AutoGG.h"
#include "modules/misc/AutoText.h"
#include "modules/misc/TimeChanger.h"
#include "modules/misc/ToggleSprintSneak.h"

namespace Glacier {

Client& Client::get() {
    static Client C;
    return C;
}

void Client::start() {
    if (running_.exchange(true)) return;

    auto logPath = Util::appDataPath() + L"\\glacier.log";
    Logger::get().open(logPath);
    Logger::get().info("Boot", "Glacier ", Glacier::kVersion, " - ", Util::wideToUtf8(logPath));

    sdk::Addresses::get().resolve();

    if (!HookManager::get().initialize()) {
        Logger::get().error("Boot", "MinHook init failed");
        running_ = false;
        return;
    }

    if (!SwapChainHook::get().install()) {
        Logger::get().error("Boot", "SwapChain hook install failed");
        running_ = false;
        return;
    }

    registerModules();
    Config::get().load();

    // Register the click-GUI as an event listener.
    ClickGui::get().init();

    Logger::get().info("Boot", "Startup complete");
}

void Client::stop() {
    if (!running_.exchange(false)) return;
    Logger::get().info("Boot", "Shutting down");
    Config::get().save();
    HookManager::get().disableAll();
    SwapChainHook::get().uninstall();
    WndProcHook::get().uninstall();
    HookManager::get().shutdown();
    Logger::get().close();
}

void Client::registerModules() {
    auto& M = ModuleManager::get();

    // HUD
    M.add<modules::Watermark>();
    M.add<modules::FPSCounter>();
    M.add<modules::Coordinates>();
    M.add<modules::Keystrokes>();
    M.add<modules::CPSCounter>();
    M.add<modules::ArmorHUD>();
    M.add<modules::ClientBranding>();
    M.add<modules::PlayerList>();

    // Visual
    M.add<modules::Crosshair>();
    M.add<modules::ChunkBorders>();
    M.add<modules::BlockOutline>();
    M.add<modules::Fullbright>();
    M.add<modules::Zoom>();
    M.add<modules::FreeLook>();
    M.add<modules::NoHurtCam>();

    // Misc
    M.add<modules::AutoGG>();
    M.add<modules::AutoText>();
    M.add<modules::TimeChanger>();
    M.add<modules::ToggleSprintSneak>();
}

} // namespace Glacier
