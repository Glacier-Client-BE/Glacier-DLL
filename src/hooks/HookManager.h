#pragma once
#include <MinHook.h>
#include <vector>

struct HookEntry {
    void*  target;
    void*  detour;
    void** original;
};

class HookManager {
public:
    static HookManager& get();

    bool init();
    void shutdown();

    bool install(void* target, void* detour, void** original);
    void enableAll();
    void disableAll();

private:
    HookManager() = default;
    HookManager(const HookManager&) = delete;
    HookManager& operator=(const HookManager&) = delete;

    std::vector<HookEntry> m_hooks;
};
