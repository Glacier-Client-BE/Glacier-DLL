#pragma once
#include <Windows.h>

namespace Glacier {

// Subclasses the Minecraft window so we can route input through ImGui and
// dispatch KeyEvent / MouseMoveEvent on the bus.
class WndProcHook {
public:
    static WndProcHook& get();

    bool install(HWND hwnd);
    void uninstall();

    HWND window() const { return hwnd_; }

private:
    WndProcHook() = default;
    ~WndProcHook() { uninstall(); }

    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);

    HWND      hwnd_ = nullptr;
    WNDPROC   prev_ = nullptr;
};

} // namespace Glacier
