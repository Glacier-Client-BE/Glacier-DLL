#include "HookManager.h"
#include "../utils/Logger.h"

HookManager& HookManager::get() {
    static HookManager instance;
    return instance;
}

bool HookManager::init() {
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        Logger::error("MH_Initialize failed: {}", MH_StatusToString(status));
        return false;
    }
    Logger::info("HookManager initialized");
    return true;
}

void HookManager::shutdown() {
    disableAll();
    MH_Uninitialize();
    m_hooks.clear();
}

bool HookManager::install(void* target, void* detour, void** original) {
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        Logger::error("MH_CreateHook failed: {}", MH_StatusToString(status));
        return false;
    }
    m_hooks.push_back({ target, detour, original });
    return true;
}

void HookManager::enableAll() {
    MH_EnableHook(MH_ALL_HOOKS);
}

void HookManager::disableAll() {
    MH_DisableHook(MH_ALL_HOOKS);
}
