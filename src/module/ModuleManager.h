#pragma once

#include <mutex>
#include <string_view>
#include <vector>

#include "Module.h"

// Owns every module instance and provides the lookup/iteration surface that the
// rest of the client (UI bridge, keybind dispatcher, render loop) talks to.
namespace glacier {

class ModuleManager {
public:
    static ModuleManager& get() {
        static ModuleManager instance;
        return instance;
    }

    void initialize();   // registers all built-in modules
    void shutdown();

    // Per-frame fan-out — only enabled modules pay the cost.
    void tickAll();
    void renderAll();

    // Dispatches a virtual-key press to any module bound to it. Returns true if
    // a module consumed the key.
    bool handleKey(int vk);

    // Fans a mouse press out to every enabled module (CPS counter, etc.).
    void handleClick(bool rightButton);

    Module* find(std::string_view name);
    bool toggle(std::string_view name);

    const std::vector<ModulePtr>& modules() const { return m_modules; }
    std::mutex& mutex() { return m_mutex; }

private:
    ModuleManager() = default;

    template <typename T, typename... Args>
    void registerModule(Args&&... args) {
        m_modules.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

    std::vector<ModulePtr> m_modules;
    std::mutex             m_mutex;   // guards toggles coming off the UI thread
};

} // namespace glacier
