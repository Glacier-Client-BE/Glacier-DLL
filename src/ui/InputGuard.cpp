#include "InputGuard.h"

#include "../hook/D3DHook.h"
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

// Virtual cursor, accumulated from raw deltas while active — see InputGuard.h.
// g_lastPt is touched only from inside mouseProc, which always runs on the
// same (hook pump) thread, so it needs no synchronization of its own;
// g_haveLastPt is what hands off "is g_lastPt valid yet" safely to and from
// setActive(), which can run on the logic or render thread.
std::atomic<float> g_cursorX{ 0.0f };
std::atomic<float> g_cursorY{ 0.0f };
std::atomic<bool>  g_haveLastPt{ false };
POINT              g_lastPt{};

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
        const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        switch (wParam) {
            case WM_MOUSEMOVE:
                // `pt` is valid here regardless of whether we block the event
                // below — it's the position this move WOULD have produced,
                // computed before the hook chain runs. Diffing it against the
                // last move gives the same raw delta the game would have
                // seen, without letting the game (or the OS cursor) see it.
                if (g_haveLastPt.exchange(true, std::memory_order_relaxed)) {
                    g_cursorX.fetch_add(static_cast<float>(info->pt.x - g_lastPt.x),
                                        std::memory_order_relaxed);
                    g_cursorY.fetch_add(static_cast<float>(info->pt.y - g_lastPt.y),
                                        std::memory_order_relaxed);
                }
                g_lastPt = info->pt;
                return 1;

            // Attack/use.
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
    const bool was = g_active.exchange(active, std::memory_order_relaxed);
    if (active && !was) {
        // Seed the virtual cursor from the real one so it doesn't jump to
        // wherever it last was the previous time the menu was open.
        POINT p{};
        if (GetCursorPos(&p)) {
            if (HWND hwnd = D3DHook::get().window()) {
                ScreenToClient(hwnd, &p);
            }
            g_cursorX.store(static_cast<float>(p.x), std::memory_order_relaxed);
            g_cursorY.store(static_cast<float>(p.y), std::memory_order_relaxed);
        }
        // The next WM_MOUSEMOVE establishes a fresh baseline instead of
        // diffing against a g_lastPt from before the menu was closed.
        g_haveLastPt.store(false, std::memory_order_relaxed);
    }
}

std::pair<float, float> InputGuard::cursorPos() {
    return { g_cursorX.load(std::memory_order_relaxed), g_cursorY.load(std::memory_order_relaxed) };
}

} // namespace glacier::ui
