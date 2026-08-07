#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <MinHook.h>

#include "../util/Logger.h"

// Thin RAII-ish wrapper over MinHook. Every hook installed through the manager
// is tracked so shutdown() can remove all of them in one pass — critical for an
// internal client that must cleanly detach without leaving trampolines behind
// (a dangling trampoline = instant crash on the next call after unload).
namespace glacier {

class HookManager {
public:
    static HookManager& get() {
        static HookManager instance;
        return instance;
    }

    bool initialize() {
        if (m_initialized) return true;
        if (MH_Initialize() != MH_OK) {
            LOG_ERROR("MH_Initialize failed");
            return false;
        }
        m_initialized = true;
        return true;
    }

    // Installs and enables a hook. `original` receives the trampoline used to
    // call the un-hooked function.
    template <typename Fn>
    bool create(const std::string& name, void* target, Fn detour, Fn* original) {
        if (!m_initialized) return false;

        // Logged before the patch, not just after it. Installing a hook writes
        // over the target and suspends threads to do it, so a bad address can
        // hang or corrupt the game *inside* this call — and a success-only log
        // line leaves that looking like the previous step was the last thing
        // that ran. This line is the difference between "died somewhere after
        // config load" and "died installing this specific hook".
        LOG_TRACE("installing hook '{}' @ {:#x}", name,
                  reinterpret_cast<std::uintptr_t>(target));

        if (MH_CreateHook(target, reinterpret_cast<void*>(detour),
                          reinterpret_cast<void**>(original)) != MH_OK) {
            LOG_ERROR("MH_CreateHook failed for '{}'", name);
            return false;
        }
        if (MH_EnableHook(target) != MH_OK) {
            LOG_ERROR("MH_EnableHook failed for '{}'", name);
            return false;
        }
        m_hooks.push_back({ name, target });
        LOG_INFO("hooked '{}' @ {:#x}", name, reinterpret_cast<std::uintptr_t>(target));
        return true;
    }

    // Hooks a vtable slot in-place (used for D3D when not relying on kiero's own
    // detour table). Returns the previous function pointer.
    void* hookVTable(void** vtable, int index, void* detour) {
        DWORD oldProtect;
        VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        void* original = vtable[index];
        vtable[index] = detour;
        VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &oldProtect);
        m_vtableHooks.push_back({ vtable, index, original });
        return original;
    }

    void shutdown() {
        if (!m_initialized) return;

        for (auto it = m_vtableHooks.rbegin(); it != m_vtableHooks.rend(); ++it) {
            DWORD oldProtect;
            VirtualProtect(&it->vtable[it->index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
            it->vtable[it->index] = it->original;
            VirtualProtect(&it->vtable[it->index], sizeof(void*), oldProtect, &oldProtect);
        }
        m_vtableHooks.clear();

        MH_DisableHook(MH_ALL_HOOKS);
        for (const auto& h : m_hooks) {
            MH_RemoveHook(h.target);
        }
        m_hooks.clear();

        MH_Uninitialize();
        m_initialized = false;
        LOG_INFO("all hooks removed");
    }

private:
    HookManager() = default;

    struct HookEntry {
        std::string name;
        void*       target;
    };
    struct VTableEntry {
        void** vtable;
        int    index;
        void*  original;
    };

    bool                     m_initialized = false;
    std::vector<HookEntry>   m_hooks;
    std::vector<VTableEntry> m_vtableHooks;
};

} // namespace glacier
