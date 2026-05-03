#include "ModuleManager.h"

namespace Glacier {

ModuleManager& ModuleManager::get() {
    static ModuleManager M;
    return M;
}

std::vector<Module*> ModuleManager::byCategory(Category cat) const {
    std::vector<Module*> out;
    for (auto& m : modules_) if (m->category() == cat) out.push_back(m.get());
    return out;
}

void ModuleManager::clear() {
    modules_.clear();
    byId_.clear();
}

} // namespace Glacier
