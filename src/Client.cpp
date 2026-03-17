#include "Client.h"
#include "hooks/HookManager.h"
#include "hooks/SwapChainHook.h"
#include "modules/ModuleManager.h"
#include "render/Renderer.h"
#include "utils/Logger.h"
#include "utils/ClientConfig.h"
#include "utils/SigScanner.h"
#include "sdk/ClientInstance.h"
#include <thread>
#include <chrono>

Client& Client::get() { static Client i; return i; }

static void resolveSDKPointers() {
    // ── ClientInstance ────────────────────────────────────────────────────────
    // Pattern: "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 80 ?? ?? ?? ??"
    uintptr_t addr = SigScanner::scan(
        "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 80 ?? ?? ?? ??");
    if (addr) {
        uintptr_t ptrAddr = SigScanner::resolveRip(addr, 3, 7);
        ClientInstance::instancePtr() = *reinterpret_cast<ClientInstance**>(ptrAddr);
        Logger::info("ClientInstance resolved at 0x{:X}", ptrAddr);
    } else {
        Logger::warn("ClientInstance sig not found — SDK calls will be no-ops");
    }

    // ── Options ───────────────────────────────────────────────────────────────
    // Pattern for Options singleton (gamma/FOV owner)
    uintptr_t optsAddr = SigScanner::scan(
        "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? F3 0F 11 81");
    if (optsAddr) {
        uintptr_t ptrAddr = SigScanner::resolveRip(optsAddr, 3, 7);
        Options::instancePtr() = *reinterpret_cast<Options**>(ptrAddr);
        Logger::info("Options resolved at 0x{:X}", ptrAddr);
    }
}

void Client::init(HMODULE hModule) {
    m_module  = hModule;
    m_running = true;

    Logger::init();
    Logger::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Logger::info("  Glacier Client v{}  ", GLACIER_VERSION);
    Logger::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    ClientConfig::get().init(hModule);

    if (!HookManager::get().init()) {
        Logger::error("HookManager failed — aborting");
        shutdown();
        return;
    }

    // Resolve SDK pointers before initialising modules
    resolveSDKPointers();

    ModuleManager::get().init();

    if (!SwapChainHook::get().init()) {
        Logger::error("SwapChainHook failed — aborting");
        shutdown();
        return;
    }

    Logger::info("Ready.  [{} ] = menu   [END] = unload",
                 static_cast<char>(ClientConfig::get().menuKey));

    while (m_running) {
        if (GetAsyncKeyState(VK_END) & 1) shutdown();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void Client::shutdown() {
    if (!m_running) return;
    m_running = false;
    Logger::info("Glacier Client shutting down…");
    SwapChainHook::get().shutdown();
    ModuleManager::get().shutdown();
    Renderer::get().shutdown();
    HookManager::get().shutdown();
    Logger::info("Goodbye.");
    Logger::shutdown();
    FreeLibraryAndExitThread(m_module, 0);
}
