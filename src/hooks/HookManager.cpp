#include "HookManager.h"
#include "../core/Logger.h"
#include <MinHook.h>

namespace Glacier {

HookManager& HookManager::get() {
    static HookManager M;
    return M;
}

HookManager::~HookManager() { shutdown(); }

bool HookManager::initialize() {
    if (initialized_) return true;
    auto r = MH_Initialize();
    if (r != MH_OK && r != MH_ERROR_ALREADY_INITIALIZED) {
        Logger::get().error("Hook", "MH_Initialize failed: ", static_cast<int>(r));
        return false;
    }
    initialized_ = true;
    return true;
}

void HookManager::shutdown() {
    if (!initialized_) return;
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    targets_.clear();
    initialized_ = false;
}

bool HookManager::create(const std::string& key, void* target, void* detour, void** original) {
    if (!initialized_ && !initialize()) return false;
    auto r = MH_CreateHook(target, detour, original);
    if (r != MH_OK) {
        Logger::get().error("Hook", "Create failed (", key, "): ", static_cast<int>(r));
        return false;
    }
    targets_[key] = target;
    return true;
}

bool HookManager::enable(const std::string& key) {
    auto it = targets_.find(key);
    if (it == targets_.end()) return false;
    return MH_EnableHook(it->second) == MH_OK;
}

bool HookManager::disable(const std::string& key) {
    auto it = targets_.find(key);
    if (it == targets_.end()) return false;
    return MH_DisableHook(it->second) == MH_OK;
}

bool HookManager::enableAll()  { return MH_EnableHook(MH_ALL_HOOKS)  == MH_OK; }
bool HookManager::disableAll() { return MH_DisableHook(MH_ALL_HOOKS) == MH_OK; }

} // namespace Glacier
