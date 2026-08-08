#include "InputGuard.h"

#include "../util/Logger.h"

#include <atomic>
#include <thread>
#include <Windows.h>

namespace glacier::ui {

namespace {

// Only the keys that move/act the player. Everything else (chat, other
// keybinds a user might still want to reach with the menu open) passes
// through untouched.
constexpr int kBlockedKeys[] = {
    'W', 'A', 'S', 'D',
    VK_SPACE,
    VK_SHIFT, VK_LSHIFT, VK_RSHIFT,
    VK_CONTROL, VK_LCONTROL, VK_RCONTROL,
};

bool isBlockedKey(int vk) {
    for (int k : kBlockedKeys) {
        if (k == vk) return true;
    }
    return false;
}

std::atomic<bool> g_active{ false };

HHOOK              g_keyHook   = nullptr;
HHOOK              g_mouseHook = nullptr;
std::thread        g_thread;
std::atomic<DWORD> g_threadId{ 0 };
std::atomic<bool>  g_running{ false };

LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_active.load(std::memory_order_relaxed)) {
        const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (isBlockedKey(static_cast<int>(info->vkCode))) {
            return 1;
        }
    }
    return CallNextHookEx(g_keyHook, code, wParam, lParam);
}

LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_active.load(std::memory_order_relaxed)) {
        switch (wParam) {
            // Attack/use. Movement (WM_MOUSEMOVE) is deliberately left alone —
            // see InputGuard.h for why.
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
                return 1;
            default:
                break;
        }
    }
    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

void hookThread() {
    g_threadId.store(GetCurrentThreadId(), std::memory_order_release);

    g_keyHook   = SetWindowsHookExW(WH_KEYBOARD_LL, &keyboardProc, GetModuleHandleW(nullptr), 0);
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL,    &mouseProc,    GetModuleHandleW(nullptr), 0);
    if (!g_keyHook || !g_mouseHook) {
        LOG_ERROR("InputGuard: SetWindowsHookEx failed ({})", GetLastError());
    }

    MSG msg;
    while (g_running.load(std::memory_order_relaxed) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_keyHook)   UnhookWindowsHookEx(g_keyHook);
    if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);
    g_keyHook = nullptr;
    g_mouseHook = nullptr;
}

} // namespace

void InputGuard::start() {
    if (g_running.exchange(true)) return;
    g_active.store(false, std::memory_order_relaxed);
    g_threadId.store(0, std::memory_order_release);
    g_thread = std::thread(&hookThread);
    LOG_INFO("InputGuard active (low-level input hook, gated on the menu)");
}

void InputGuard::stop() {
    if (!g_running.exchange(false)) return;
    g_active.store(false, std::memory_order_relaxed);
    if (const DWORD id = g_threadId.load(std::memory_order_acquire)) {
        PostThreadMessageW(id, WM_NULL, 0, 0);
    }
    if (g_thread.joinable()) g_thread.join();
}

void InputGuard::setActive(bool active) {
    g_active.store(active, std::memory_order_relaxed);
}

} // namespace glacier::ui
