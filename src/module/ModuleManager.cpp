#include "ModuleManager.h"

#include "ModuleRegistry.h"
#include "../util/Logger.h"

#include <algorithm>

namespace glacier {

void ModuleManager::initialize() {
    std::scoped_lock lock(m_mutex);
    if (m_initialized) return;

    const auto& factories = ModuleRegistry::get().factories();
    m_modules.reserve(factories.size());

    for (const auto& make : factories) {
        if (auto module = make()) {
            m_modules.push_back(std::move(module));
        }
    }

    // Stable, category-then-name ordering so the menu doesn't reshuffle between
    // runs (static-init order across translation units is unspecified).
    std::sort(m_modules.begin(), m_modules.end(),
              [](const ModulePtr& a, const ModulePtr& b) {
                  if (a->category() != b->category()) {
                      return a->category() < b->category();
                  }
                  return a->name() < b->name();
              });

    m_initialized = true;
    LOG_INFO("registered {} module(s)", m_modules.size());
    for (const auto& m : m_modules) {
        LOG_TRACE("  [{}] {}", categoryName(m->category()), m->name());
    }
}

void ModuleManager::shutdown() {
    std::scoped_lock lock(m_mutex);
    // Disable before destroying so each module gets the chance to undo whatever
    // it did to the game (restore gamma, remove patches, …). Skipping this is
    // how an unloaded client leaves the game visibly broken.
    for (auto& m : m_modules) {
        if (m->enabled()) {
            m->setEnabled(false);
        }
    }
    m_modules.clear();
    m_initialized = false;
}

void ModuleManager::tickAll() {
    std::scoped_lock lock(m_mutex);
    for (auto& m : m_modules) {
        m->onTick();
    }
}

void ModuleManager::renderAll() {
    std::scoped_lock lock(m_mutex);
    for (auto& m : m_modules) {
        if (m->enabled()) {
            m->onRender();
        }
    }
}

bool ModuleManager::handleKey(int vk) {
    if (vk == 0) return false;
    std::scoped_lock lock(m_mutex);

    bool consumed = false;
    for (auto& m : m_modules) {
        if (m->keybind() == vk) {
            m->toggle();
            LOG_INFO("{} {}", m->name(), m->enabled() ? "enabled" : "disabled");
            consumed = true;
        }
    }
    return consumed;
}

void ModuleManager::handleClick(bool rightButton) {
    std::scoped_lock lock(m_mutex);
    for (auto& m : m_modules) {
        if (m->enabled()) {
            m->onClick(rightButton);
        }
    }
}

Module* ModuleManager::find(std::string_view name) {
    std::scoped_lock lock(m_mutex);
    for (auto& m : m_modules) {
        if (m->name() == name) return m.get();
    }
    return nullptr;
}

} // namespace glacier
