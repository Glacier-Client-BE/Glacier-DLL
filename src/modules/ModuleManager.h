#pragma once
#include "Module.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace Glacier {

class ModuleManager {
public:
    static ModuleManager& get();

    // Construct + register. Returns the new module.
    template <class T, class... Args>
    T* add(Args&&... args) {
        auto m = std::make_unique<T>(std::forward<Args>(args)...);
        T* ref = m.get();
        byId_[ref->id()] = ref;
        modules_.push_back(std::move(m));
        return ref;
    }

    // Lookup by id.  Returns nullptr if missing.
    [[nodiscard]] Module* find(const std::string& id) const {
        auto it = byId_.find(id);
        return it == byId_.end() ? nullptr : it->second;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<Module>>& all() const { return modules_; }
    [[nodiscard]] std::vector<Module*> byCategory(Category cat) const;

    void clear();

private:
    ModuleManager() = default;
    std::vector<std::unique_ptr<Module>>    modules_;
    std::unordered_map<std::string, Module*> byId_;
};

} // namespace Glacier
