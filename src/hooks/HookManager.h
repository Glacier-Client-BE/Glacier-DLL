#pragma once
#include <Windows.h>
#include <unordered_map>
#include <string>
#include <vector>

namespace Glacier {

// Wrapper around MinHook with a string-key registry of named trampolines.
class HookManager {
public:
    static HookManager& get();

    bool initialize();
    void shutdown();

    // Create a trampoline for a static function. `original` receives the
    // pointer-to-original; pass &your_local_orig_fn cast to void**.
    bool create(const std::string& key, void* target, void* detour, void** original);

    // Enable/disable by key.
    bool enable (const std::string& key);
    bool disable(const std::string& key);

    // Enable everything created so far.
    bool enableAll();
    bool disableAll();

private:
    HookManager() = default;
    ~HookManager();

    bool initialized_ = false;
    std::unordered_map<std::string, void*> targets_; // key -> hooked target
};

} // namespace Glacier
