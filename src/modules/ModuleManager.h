#pragma once
#include "ModuleBase.h"
#include <memory>
#include <vector>

class ModuleManager {
public:
    static ModuleManager& get();

    void init();
    void shutdown();
    void onTick();
    void onRender(ImDrawList* dl);
    void onRenderImGui();

    template<typename T, typename... Args>
    T* addModule(Args&&... args) {
        auto  m   = std::make_shared<T>(std::forward<Args>(args)...);
        T*    raw = m.get();
        m_modules.emplace_back(std::move(m));
        return raw;
    }

    const std::vector<std::shared_ptr<ModuleBase>>& getModules() const { return m_modules; }

    // Convenience: find a module by type
    template<typename T>
    T* find() const {
        for (auto& m : m_modules)
            if (auto* p = dynamic_cast<T*>(m.get())) return p;
        return nullptr;
    }

private:
    ModuleManager() = default;
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;
    std::vector<std::shared_ptr<ModuleBase>> m_modules;
};
