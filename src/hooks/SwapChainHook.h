#pragma once
#include <d3d11.h>

class SwapChainHook {
public:
    static SwapChainHook& get();

    bool init();
    void shutdown();

private:
    SwapChainHook() = default;
    SwapChainHook(const SwapChainHook&) = delete;
    SwapChainHook& operator=(const SwapChainHook&) = delete;
};
