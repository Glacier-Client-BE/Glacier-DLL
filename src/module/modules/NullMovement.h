#pragma once

#include <atomic>
#include <thread>
#include <Windows.h>

#include "../Module.h"
#include "../../hook/D3DHook.h"
#include "../../util/Logger.h"

namespace glacier {

// Null movement (a.k.a. snap tap / SOCD "last input wins"): when both keys of
// an opposing pair (A+D, or W+S) are physically held, the most recent press
// wins instead of the inputs cancelling out.
//
// Implementation: a WH_KEYBOARD_LL hook on a dedicated message-pump thread
// observes *physical* key state (injected events are ignored via
// LLKHF_INJECTED) and injects the matching release/re-press with SendInput.
// This works below the game's raw-input reads, needs no game offsets, and is
// fully reverted on disable. The hook only intervenes while the Minecraft
// window is in the foreground.
class NullMovement final : public Module {
public:
    NullMovement()
        : Module("Null Movement", "Opposing movement keys: last input wins", Category::Movement) {
        addSetting(Setting{ "horizontal", "Horizontal (A/D)", true });
        addSetting(Setting{ "vertical", "Vertical (W/S)", false });
        s_instance = this;
    }

    ~NullMovement() override {
        stopHookThread();
        s_instance = nullptr;
    }

    void onEnable() override {
        if (m_thread.joinable()) return;
        s_running.store(true);
        m_thread = std::thread([] { hookThreadMain(); });
    }

    void onDisable() override { stopHookThread(); }

private:
    // ── Per-key physical/suppression state, indexed by virtual-key code ──
    struct KeyState {
        bool physicallyDown = false;   // what the user's finger is doing
        bool suppressed     = false;   // we injected a release for this key
    };

    static int opposite(int vk) {
        switch (vk) {
            case 'A': return 'D';
            case 'D': return 'A';
            case 'W': return 'S';
            case 'S': return 'W';
        }
        return 0;
    }

    bool pairEnabled(int vk) const {
        const char* id = (vk == 'A' || vk == 'D') ? "horizontal" : "vertical";
        const auto* s = setting(id);
        return s && s->asBool();
    }

    static void inject(int vk, bool down) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(vk);
        in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
        in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(INPUT));
    }

    static bool gameInForeground() {
        const HWND game = D3DHook::get().window();
        return game != nullptr && GetForegroundWindow() == game;
    }

    static LRESULT CALLBACK lowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
        if (code != HC_ACTION || !s_instance) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }

        const auto* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (kb->flags & LLKHF_INJECTED) {                 // ignore our own events
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }

        const int vk = static_cast<int>(kb->vkCode);
        const int opp = opposite(vk);
        if (opp == 0) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }

        const bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        auto& self = *s_instance;
        auto& me   = self.stateFor(vk);
        auto& other = self.stateFor(opp);

        const bool wasDown = me.physicallyDown;
        me.physicallyDown = down;

        if (!self.enabled() || !self.pairEnabled(vk) || !gameInForeground()) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }

        if (down && !wasDown) {
            // New press: if the opposite key is physically held, release it so
            // this press wins immediately.
            if (other.physicallyDown && !other.suppressed) {
                other.suppressed = true;
                inject(opp, false);
            }
            me.suppressed = false;
        } else if (!down) {
            // Release: if we had suppressed the opposite key and the user still
            // holds it, restore it so movement continues their way.
            me.suppressed = false;
            if (other.physicallyDown && other.suppressed) {
                other.suppressed = false;
                inject(opp, true);
            }
        }

        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    KeyState& stateFor(int vk) {
        switch (vk) {
            case 'A': return m_keys[0];
            case 'D': return m_keys[1];
            case 'W': return m_keys[2];
            default:  return m_keys[3];   // 'S'
        }
    }

    static void hookThreadMain() {
        // Create the message queue up-front so stopHookThread's WM_QUIT can
        // never race the hook installation.
        MSG peek;
        PeekMessageW(&peek, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        s_threadId = GetCurrentThreadId();

        s_hook = SetWindowsHookExW(WH_KEYBOARD_LL, &lowLevelKeyboardProc,
                                   GetModuleHandleW(nullptr), 0);
        if (!s_hook) {
            LOG_WARN("NullMovement: SetWindowsHookEx failed ({})", GetLastError());
            s_threadId = 0;
            return;
        }

        // LL hooks deliver through this thread's message queue.
        MSG msg;
        while (s_running.load() && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        UnhookWindowsHookEx(s_hook);
        s_hook = nullptr;
        s_threadId = 0;
    }

    void stopHookThread() {
        if (!m_thread.joinable()) return;
        s_running.store(false);
        // The thread publishes its id before entering the message loop; wait
        // briefly for that so the WM_QUIT can't be lost.
        for (int i = 0; i < 100 && s_threadId.load() == 0; ++i) Sleep(1);
        if (const DWORD tid = s_threadId.load()) {
            PostThreadMessageW(tid, WM_QUIT, 0, 0);
        }
        m_thread.join();

        // Undo any suppression still in effect so no key sticks released.
        for (int vk : { 'A', 'D', 'W', 'S' }) {
            auto& st = stateFor(vk);
            if (st.suppressed && st.physicallyDown) inject(vk, true);
            st.suppressed = false;
            st.physicallyDown = false;
        }
    }

    KeyState m_keys[4]{};
    std::thread m_thread;

    inline static NullMovement*      s_instance = nullptr;
    inline static HHOOK              s_hook     = nullptr;
    inline static std::atomic<DWORD> s_threadId{ 0 };
    inline static std::atomic<bool>  s_running{ false };
};

} // namespace glacier
