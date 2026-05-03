#include "WndProcHook.h"
#include "../../core/Logger.h"
#include "../../events/EventBus.h"
#include "../../events/Events.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace Glacier {

WndProcHook& WndProcHook::get() {
    static WndProcHook W;
    return W;
}

bool WndProcHook::install(HWND hwnd) {
    if (prev_) return true;
    hwnd_ = hwnd;
    prev_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProcHook::proc)));
    Logger::get().info("DX", "WndProc subclass installed");
    return prev_ != nullptr;
}

void WndProcHook::uninstall() {
    if (prev_ && hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(prev_));
        prev_ = nullptr;
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK WndProcHook::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto& self = WndProcHook::get();

    // Always feed ImGui first so its IO state is current.
    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);

    auto& io = ImGui::GetIO();
    bool capture = io.WantCaptureKeyboard || io.WantCaptureMouse;

    auto fireKey = [&](int vk, bool down) {
        KeyEvent e;
        e.vkey = vk;
        e.down = down;
        e.consumedByGui = capture;
        EventBus::get().dispatch(e);
        return e.isCancelled();
    };

    switch (msg) {
        case WM_KEYDOWN:    case WM_SYSKEYDOWN: { if (fireKey(static_cast<int>(wp), true ) && capture) return 0; break; }
        case WM_KEYUP:      case WM_SYSKEYUP:   { if (fireKey(static_cast<int>(wp), false) && capture) return 0; break; }
        case WM_LBUTTONDOWN:{ if (fireKey(VK_LBUTTON, true ) && capture) return 0; break; }
        case WM_LBUTTONUP:  { if (fireKey(VK_LBUTTON, false) && capture) return 0; break; }
        case WM_RBUTTONDOWN:{ if (fireKey(VK_RBUTTON, true ) && capture) return 0; break; }
        case WM_RBUTTONUP:  { if (fireKey(VK_RBUTTON, false) && capture) return 0; break; }
        case WM_MBUTTONDOWN:{ if (fireKey(VK_MBUTTON, true ) && capture) return 0; break; }
        case WM_MBUTTONUP:  { if (fireKey(VK_MBUTTON, false) && capture) return 0; break; }
        case WM_MOUSEMOVE: {
            // Forward the delta as a MouseMove event (ImGui already has absolute pos).
            static int lastX = 0, lastY = 0;
            int x = LOWORD(lp), y = HIWORD(lp);
            MouseMoveEvent e;
            e.dx = static_cast<float>(x - lastX);
            e.dy = static_cast<float>(y - lastY);
            lastX = x; lastY = y;
            EventBus::get().dispatch(e);
            break;
        }
    }

    if (capture && (msg == WM_CHAR || msg == WM_KEYDOWN || msg == WM_KEYUP)) {
        // Eat keystrokes only when the GUI explicitly wants them.
        return 0;
    }

    return CallWindowProcW(self.prev_, hwnd, msg, wp, lp);
}

} // namespace Glacier
