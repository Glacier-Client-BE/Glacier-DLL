#include "../Module.h"
#include "../ModuleRegistry.h"
#include "../../util/Logger.h"

#include <atomic>
#include <thread>
#include <Windows.h>

namespace glacier {

namespace {

// ── Null movement ("last key wins") ──
//
// Pressing A while D is held normally cancels out to standing still. This makes
// the most recently pressed direction win instead, and restores the other one
// when the newer key is released.
//
// Implemented entirely as an OS-level input remapper: WH_KEYBOARD_LL, no game
// memory, no hooks into Minecraft at all. It therefore works on any build, and
// is unaffected by signature drift.
//
// ── Why a dedicated thread ──
// A low-level keyboard hook's callback is delivered through the message queue
// of the thread that installed it, so that thread MUST pump messages. Glacier's
// logic loop only sleeps, so installing the hook there would mean the callback
// never runs. Hence a thread whose entire job is GetMessage().

struct Pair {
    int a;
    int b;
};
constexpr Pair kPairs[] = { { 'A', 'D' }, { 'W', 'S' } };

// State is touched only by the hook thread, so no locking is needed.
bool g_physical[256]   = {};
bool g_suppressed[256] = {};

HHOOK             g_hook = nullptr;
std::thread       g_thread;
std::atomic<DWORD> g_threadId{ 0 };
std::atomic<bool>  g_running{ false };

int opposite(int vk) {
    for (const auto& p : kPairs) {
        if (p.a == vk) return p.b;
        if (p.b == vk) return p.a;
    }
    return 0;
}

void sendKey(int vk, bool down) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vk);
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC));
    input.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &input, sizeof(INPUT));
}

LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    // Let our own synthetic events straight through, or the state machine below
    // would react to its own output and oscillate.
    if (info->flags & LLKHF_INJECTED) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    const int vk = static_cast<int>(info->vkCode);
    const int other = opposite(vk);
    if (other == 0) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool isUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);

    if (isDown) {
        const bool wasPhysical = g_physical[vk];
        g_physical[vk] = true;

        // Newly pressed key takes priority: silence its opposite.
        if (!wasPhysical && g_physical[other] && !g_suppressed[other]) {
            g_suppressed[other] = true;
            sendKey(other, false);   // tell the game the old direction released
        }

        // This key is the active one now.
        if (g_suppressed[vk]) g_suppressed[vk] = false;

    } else if (isUp) {
        g_physical[vk] = false;

        if (g_suppressed[vk]) {
            // The game already saw this key release, when it was suppressed.
            g_suppressed[vk] = false;
            return 1;
        }

        // Releasing the winner hands control back to a still-held opposite.
        if (g_physical[other] && g_suppressed[other]) {
            g_suppressed[other] = false;
            sendKey(other, true);
        }
    }

    // Swallow everything (including auto-repeat) for a suppressed key.
    if (g_suppressed[vk]) {
        return 1;
    }

    return CallNextHookEx(g_hook, code, wParam, lParam);
}

void hookThread() {
    g_threadId.store(GetCurrentThreadId(), std::memory_order_release);

    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, &keyboardProc, GetModuleHandleW(nullptr), 0);
    if (!g_hook) {
        LOG_ERROR("Null Movement: SetWindowsHookEx failed ({})", GetLastError());
        g_running.store(false);
        return;
    }

    MSG msg;
    while (g_running.load(std::memory_order_relaxed) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;
}

} // namespace

class NullMovement final : public Module {
public:
    NullMovement()
        : Module("Null Movement", "Opposite movement keys no longer cancel out",
                 Category::Movement, 0) {}

    void onEnable() override {
        if (g_running.exchange(true)) return;

        // Clear stale state: keys held when the module was last disabled would
        // otherwise look "physically down" forever.
        for (int i = 0; i < 256; ++i) {
            g_physical[i] = false;
            g_suppressed[i] = false;
        }

        g_threadId.store(0, std::memory_order_release);
        g_thread = std::thread(&hookThread);
        LOG_INFO("Null Movement active (low-level keyboard hook)");
    }

    void onDisable() override {
        if (!g_running.exchange(false)) return;

        // Release anything currently suppressed, or the game is left believing a
        // key is up while the user is still holding it.
        for (const auto& p : kPairs) {
            for (const int vk : { p.a, p.b }) {
                if (g_suppressed[vk] && g_physical[vk]) {
                    sendKey(vk, true);
                }
                g_suppressed[vk] = false;
            }
        }

        // Nudge the pump so GetMessage returns and the thread can exit.
        if (const DWORD id = g_threadId.load(std::memory_order_acquire)) {
            PostThreadMessageW(id, WM_NULL, 0, 0);
        }
        if (g_thread.joinable()) g_thread.join();

        LOG_INFO("Null Movement stopped");
    }

    // The hook thread outlives a plain destructor if the client is torn down
    // while enabled, so make sure disable runs.
    ~NullMovement() override {
        if (enabled()) onDisable();
    }
};

GLACIER_MODULE(NullMovement);

} // namespace glacier
