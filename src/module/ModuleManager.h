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

    // Queues a toggle to be applied on the logic thread by the next tickAll().
    //
    // The menu draws — and therefore handles its clicks — inside the Present
    // hook, on the GAME'S RENDER THREAD, and onEnable/onDisable are allowed to
    // do things that must never happen there. NullMovement::onDisable is the
    // proof: it calls SendInput and then *joins* its low-level hook thread.
    // Blocking a render thread inside Present on a thread that is itself
    // servicing OS input hooks is the deadlock shape architecture fact #9 in
    // docs/HANDOFF.md warns about, and it became easy to hit the moment the
    // redesigned menu made a module's whole card a toggle.
    //
    // So the UI states an intent and the logic thread carries it out. The
    // resulting one-tick (10ms) lag before the card updates is imperceptible;
    // the alternative is a class of hang that only some modules can cause.
    void requestToggle(Module* module);

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

    // Toggles requested from the render thread, drained by tickAll(). Its own
    // lock rather than m_mutex: the render thread must not be able to block on
    // whatever the logic thread is doing inside a module's onTick().
    std::mutex           m_pendingMutex;
    std::vector<Module*> m_pendingToggles;
};

} // namespace glacier
