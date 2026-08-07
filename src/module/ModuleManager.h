#pragma once

#include <mutex>
#include <string_view>
#include <vector>

#include "Module.h"

// Owns every module instance and fans the client's lifecycle events out to
// them. Modules are constructed from ModuleRegistry at initialize() — there is
// no hand-written registration list.
//
// Locking: the module *list* is built once and never mutated afterwards, but
// dispatch happens from two different threads (the logic thread drives
// tickAll(), the render thread drives renderAll() from inside Present), so the
// list is guarded. Individual modules are responsible for their own internal
// state; anything they share with a hook detour should be atomic.
namespace glacier {

class ModuleManager {
public:
    static ModuleManager& get() {
        static ModuleManager instance;
        return instance;
    }

    // Instantiates every registered module. Safe to call once; repeated calls
    // are ignored.
    void initialize();
    void shutdown();

    void tickAll();
    void renderAll();

    // Dispatches a key press to any module bound to it. Returns true if at
    // least one module consumed the key.
    bool handleKey(int vk);
    void handleClick(bool rightButton);

    // Lookup by display name (used by the menu and by keybind persistence).
    Module* find(std::string_view name);

    // Direct access for the UI. The caller must hold nothing — the returned
    // reference is stable for the lifetime of the manager because the vector is
    // never resized after initialize().
    const std::vector<ModulePtr>& modules() const { return m_modules; }

    std::size_t count() const { return m_modules.size(); }

private:
    ModuleManager() = default;

    mutable std::recursive_mutex m_mutex;
    std::vector<ModulePtr> m_modules;
    bool m_initialized = false;
};

} // namespace glacier
