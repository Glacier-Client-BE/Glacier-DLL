#include "InputGuard.h"

#include "../hook/D3DHook.h"
#include "../hook/HookManager.h"
#include "../util/Logger.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace glacier::ui {

namespace {

// ── Movement keys ────────────────────────────────────────────────────────────
//
// Only the keys that move/act the player. Everything else (chat, other
// keybinds a user might still want to reach with the menu open) passes through
// untouched.
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

std::atomic<bool> g_menuOpen{ false };

// ── Low-level keyboard hook ──────────────────────────────────────────────────
//
// The mouse half of this pair is deliberately gone. It blocked WM_MOUSEMOVE,
// which froze the OS pointer without touching the Raw Input the game actually
// steers the camera with — the worst of both worlds. Mouse suppression now
// happens in hkGetRawInputData/hkGetRawInputBuffer below.
HHOOK              g_keyHook = nullptr;
std::thread        g_thread;
std::atomic<DWORD> g_threadId{ 0 };
std::atomic<bool>  g_running{ false };

LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_menuOpen.load(std::memory_order_relaxed)) {
        const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (isBlockedKey(static_cast<int>(info->vkCode))) {
            return 1;
        }
    }
    return CallNextHookEx(g_keyHook, code, wParam, lParam);
}

void hookThread() {
    g_threadId.store(GetCurrentThreadId(), std::memory_order_release);

    g_keyHook = SetWindowsHookExW(WH_KEYBOARD_LL, &keyboardProc, GetModuleHandleW(nullptr), 0);
    if (!g_keyHook) {
        LOG_ERROR("InputGuard: SetWindowsHookEx(WH_KEYBOARD_LL) failed ({})", GetLastError());
    }

    MSG msg;
    while (g_running.load(std::memory_order_relaxed) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_keyHook) UnhookWindowsHookEx(g_keyHook);
    g_keyHook = nullptr;
}

// ── Win32 pointer + Raw Input hooks ──────────────────────────────────────────

BOOL    (WINAPI* o_ClipCursor)(const RECT*)                            = nullptr;
BOOL    (WINAPI* o_SetCursorPos)(int, int)                             = nullptr;
int     (WINAPI* o_ShowCursor)(BOOL)                                   = nullptr;
HWND    (WINAPI* o_SetCapture)(HWND)                                   = nullptr;
HCURSOR (WINAPI* o_SetCursor)(HCURSOR)                                 = nullptr;
UINT    (WINAPI* o_GetRawInputData)(HRAWINPUT, UINT, LPVOID, PUINT, UINT) = nullptr;
UINT    (WINAPI* o_GetRawInputBuffer)(PRAWINPUT, PUINT, UINT)          = nullptr;

// False once stop() has run. Everything below calls the trampolines, and
// HookManager::shutdown() frees them — so after that point our own calls have
// to go to the real API instead. (The detours themselves are removed by
// MinHook, which suspends threads to do it, so they simply stop being
// reachable; it is only *our* stored pointers that would dangle.)
std::atomic<bool> g_apiLive{ false };

// What the game last asked ClipCursor for while we were suppressing it, so
// closing the menu hands back exactly that rather than a guess. Four atomics
// rather than a lock: hkClipCursor runs on the game's threads, and the Present
// hook must never take a lock the game could be holding.
std::atomic<bool> g_haveGameClip{ false };
std::atomic<LONG> g_clipL{ 0 }, g_clipT{ 0 }, g_clipR{ 0 }, g_clipB{ 0 };

UINT g_syncMessage = 0;

// Zeroes a mouse record in place. The event still reaches the game — it just
// reports no movement and no button change, which keeps the game's own input
// state consistent in a way that dropping the record outright would not.
void neutralizeMouse(RAWINPUT* ri) {
    if (!ri || ri->header.dwType != RIM_TYPEMOUSE) return;
    ri->data.mouse.lLastX          = 0;
    ri->data.mouse.lLastY          = 0;
    ri->data.mouse.usButtonFlags   = 0;
    ri->data.mouse.usButtonData    = 0;
    ri->data.mouse.ulRawButtons    = 0;
    ri->data.mouse.ulButtons       = 0;
}

BOOL WINAPI hkClipCursor(const RECT* rect) {
    // A null rect means "release the clip", which is never something we want
    // to suppress — it can only ever free the pointer.
    if (rect && g_menuOpen.load(std::memory_order_relaxed)) {
        g_clipL.store(rect->left,   std::memory_order_relaxed);
        g_clipT.store(rect->top,    std::memory_order_relaxed);
        g_clipR.store(rect->right,  std::memory_order_relaxed);
        g_clipB.store(rect->bottom, std::memory_order_relaxed);
        g_haveGameClip.store(true, std::memory_order_release);
        // Reporting success matters: a caller told the clip failed is liable
        // to retry it every frame, or to treat it as a fatal input error.
        return TRUE;
    }
    return o_ClipCursor(rect);
}

BOOL WINAPI hkSetCursorPos(int x, int y) {
    // Bedrock re-centres the pointer every frame while the camera has it.
    // Left alone, that alone would pin our menu cursor to the middle of the
    // screen no matter how the user moved the mouse.
    if (g_menuOpen.load(std::memory_order_relaxed)) return TRUE;
    return o_SetCursorPos(x, y);
}

int WINAPI hkShowCursor(BOOL show) {
    // Swallow hides only. Letting a show through is harmless, and the count is
    // a process-wide running total — decrementing it behind our back is what
    // makes the pointer vanish a frame after the menu opens.
    if (!show && g_menuOpen.load(std::memory_order_relaxed)) return 1;
    return o_ShowCursor(show);
}

HWND WINAPI hkSetCapture(HWND hwnd) {
    if (g_menuOpen.load(std::memory_order_relaxed)) return nullptr;
    return o_SetCapture(hwnd);
}

HCURSOR WINAPI hkSetCursor(HCURSOR cursor) {
    // SetCursor(nullptr) is the other way to hide a pointer, and Bedrock uses
    // it. Substitute the arrow rather than dropping the call, so the game's
    // own bookkeeping still gets a sensible previous-cursor back.
    if (!cursor && g_menuOpen.load(std::memory_order_relaxed)) {
        return o_SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
    return o_SetCursor(cursor);
}

UINT WINAPI hkGetRawInputData(HRAWINPUT input, UINT command, LPVOID data,
                              PUINT size, UINT headerSize) {
    const UINT result = o_GetRawInputData(input, command, data, size, headerSize);
    // data == nullptr is the "how big a buffer do I need" call, which has
    // nothing to neutralize.
    if (data && command == RID_INPUT && result != static_cast<UINT>(-1)
        && g_menuOpen.load(std::memory_order_relaxed)) {
        neutralizeMouse(static_cast<RAWINPUT*>(data));
    }
    return result;
}

UINT WINAPI hkGetRawInputBuffer(PRAWINPUT data, PUINT size, UINT headerSize) {
    const UINT count = o_GetRawInputBuffer(data, size, headerSize);
    if (data && count != static_cast<UINT>(-1) && count > 0
        && g_menuOpen.load(std::memory_order_relaxed)) {
        auto* record = data;
        for (UINT i = 0; i < count; ++i) {
            neutralizeMouse(record);
            // Open-coded NEXTRAWINPUTBLOCK. The SDK macro expands to
            // RAWINPUT_ALIGN, which references QWORD — a type winnt.h does not
            // define under this project's WIN32_LEAN_AND_MEAN/UNICODE
            // configuration, so the macro simply does not compile here. The
            // alignment it applies is 8 bytes on x64, which is what this does.
            constexpr std::uintptr_t kAlign = sizeof(std::uint64_t);
            const auto next = (reinterpret_cast<std::uintptr_t>(record) + record->header.dwSize
                               + kAlign - 1) & ~(kAlign - 1);
            record = reinterpret_cast<PRAWINPUT>(next);
        }
    }
    return count;
}

// ── Cursor handoff, window thread only ───────────────────────────────────────

// Each of these prefers the trampoline (bypassing our own suppression) and
// falls back to the real API when that particular hook failed to install —
// a partial install degrades to "the game can fight us for the pointer",
// not to a null call.
BOOL    clipCursorRaw(const RECT* r) { return o_ClipCursor   ? o_ClipCursor(r)   : ClipCursor(r); }
BOOL    setCursorPosRaw(int x, int y){ return o_SetCursorPos ? o_SetCursorPos(x, y) : SetCursorPos(x, y); }
int     showCursorRaw(BOOL show)     { return o_ShowCursor   ? o_ShowCursor(show): ShowCursor(show); }
HWND    setCaptureRaw(HWND hwnd)     { return o_SetCapture   ? o_SetCapture(hwnd): SetCapture(hwnd); }
HCURSOR setCursorRaw(HCURSOR cursor) { return o_SetCursor    ? o_SetCursor(cursor) : SetCursor(cursor); }

void showCursorTo(bool visible) {
    // ShowCursor is a counter, not a flag. Both loops are self-limiting.
    if (visible) {
        while (showCursorRaw(TRUE) < 0) {}
    } else {
        while (showCursorRaw(FALSE) >= 0) {}
    }
}

void applyCursorState(bool menuOpen, HWND hwnd) {
    if (!g_apiLive.load(std::memory_order_acquire)) return;

    if (menuOpen) {
        // Order matters. Free the pointer first, then put it somewhere the
        // user can see it, then make it visible — with the hooks above live,
        // the game cannot undo any of the three until the menu closes.
        clipCursorRaw(nullptr);
        ReleaseCapture();
        if (hwnd) {
            RECT client{};
            GetClientRect(hwnd, &client);
            POINT centre{ (client.right - client.left) / 2, (client.bottom - client.top) / 2 };
            ClientToScreen(hwnd, &centre);
            setCursorPosRaw(centre.x, centre.y);
        }
        showCursorTo(true);
        setCursorRaw(LoadCursorW(nullptr, IDC_ARROW));
        return;
    }

    showCursorTo(false);
    if (hwnd) setCaptureRaw(hwnd);

    if (g_haveGameClip.load(std::memory_order_acquire)) {
        // Hand back exactly the rect the game asked for while suppressed.
        const RECT wanted{
            g_clipL.load(std::memory_order_relaxed), g_clipT.load(std::memory_order_relaxed),
            g_clipR.load(std::memory_order_relaxed), g_clipB.load(std::memory_order_relaxed),
        };
        clipCursorRaw(&wanted);
        g_haveGameClip.store(false, std::memory_order_release);
    } else if (hwnd) {
        // The game never asked while the menu was up (it does not always
        // re-clip every frame). Fall back to Flarial's grabCursor: a
        // zero-size rect at the centre of the client area, so raw motion
        // still arrives while the pointer itself cannot go anywhere. See
        // reference/flarial/src/Client/Hook/Hooks/Input/CursorHandler.hpp.
        RECT rect{};
        GetClientRect(hwnd, &rect);
        rect.top  = (rect.bottom + rect.top) / 2;
        rect.left = (rect.right + rect.left) / 2;
        ClientToScreen(hwnd, reinterpret_cast<LPPOINT>(&rect));
        rect.bottom = rect.top;
        rect.right  = rect.left;
        clipCursorRaw(&rect);
    }
}

} // namespace

void InputGuard::start() {
    if (g_running.exchange(true)) return;
    g_menuOpen.store(false, std::memory_order_relaxed);
    g_threadId.store(0, std::memory_order_release);
    g_thread = std::thread(&hookThread);
    LOG_INFO("InputGuard active (movement keys blocked while the menu is open)");
}

bool InputGuard::installApiHooks() {
    if (g_apiLive.load(std::memory_order_acquire)) return true;

    g_syncMessage = RegisterWindowMessageW(L"GlacierCursorSync");
    if (!g_syncMessage) {
        LOG_ERROR("RegisterWindowMessage failed ({}) — the menu cannot take the cursor",
                  GetLastError());
        return false;
    }

    // Resolved by name out of user32 rather than by taking &ClipCursor and
    // friends: what `&` yields for a dllimport function is a compiler detail,
    // and MinHook has to be given the real export.
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) {
        LOG_ERROR("user32.dll not loaded?! — the menu cannot take the cursor");
        return false;
    }
    const auto entry = [&](const char* name) {
        void* address = reinterpret_cast<void*>(GetProcAddress(user32, name));
        if (!address) LOG_ERROR("user32!{} not found — that hook is skipped", name);
        return address;
    };

    auto& hooks = HookManager::get();
    bool ok = true;
    // Written out one per line, each result folded in separately, so a single
    // failure does not short-circuit the rest: a partial install is far more
    // useful (and far more diagnosable) than none.
    if (void* p = entry("ClipCursor"))
        ok = hooks.create("ClipCursor", p, &hkClipCursor, &o_ClipCursor) && ok;
    else ok = false;
    if (void* p = entry("SetCursorPos"))
        ok = hooks.create("SetCursorPos", p, &hkSetCursorPos, &o_SetCursorPos) && ok;
    else ok = false;
    if (void* p = entry("ShowCursor"))
        ok = hooks.create("ShowCursor", p, &hkShowCursor, &o_ShowCursor) && ok;
    else ok = false;
    if (void* p = entry("SetCapture"))
        ok = hooks.create("SetCapture", p, &hkSetCapture, &o_SetCapture) && ok;
    else ok = false;
    if (void* p = entry("SetCursor"))
        ok = hooks.create("SetCursor", p, &hkSetCursor, &o_SetCursor) && ok;
    else ok = false;
    if (void* p = entry("GetRawInputData"))
        ok = hooks.create("GetRawInputData", p, &hkGetRawInputData, &o_GetRawInputData) && ok;
    else ok = false;
    if (void* p = entry("GetRawInputBuffer"))
        ok = hooks.create("GetRawInputBuffer", p, &hkGetRawInputBuffer, &o_GetRawInputBuffer) && ok;
    else ok = false;

    if (!ok) {
        // Partial failure is worth naming loudly: whichever one didn't install
        // is exactly the symptom the user will report (camera still turns, or
        // the pointer is still invisible), and the mapping from symptom to
        // hook is not obvious from in-game.
        LOG_ERROR("not every input hook installed — see the MH_ errors above. The menu's "
                  "cursor/camera handoff will be partial");
    } else {
        LOG_INFO("input hooks installed (pointer + raw mouse are handed to the menu on open)");
    }

    g_apiLive.store(true, std::memory_order_release);
    return ok;
}

void InputGuard::stop() {
    // Give the pointer back before anything is torn down. Synchronously, not
    // via the usual PostMessage: HookManager::shutdown() is moments away and
    // will free the trampolines applyCursorState calls through, so a message
    // still sitting in the queue would either be dropped (pointer never
    // returned) or run against freed memory. SendMessageTimeout with
    // ABORTIFHUNG means a wedged window thread costs 200ms, not a hang.
    g_menuOpen.store(false, std::memory_order_relaxed);
    if (HWND hwnd = D3DHook::get().window(); hwnd && g_syncMessage) {
        SendMessageTimeoutW(hwnd, g_syncMessage, 0, 0, SMTO_ABORTIFHUNG, 200, nullptr);
    }
    g_apiLive.store(false, std::memory_order_release);

    if (!g_running.exchange(false)) return;
    g_menuOpen.store(false, std::memory_order_relaxed);
    if (const DWORD id = g_threadId.load(std::memory_order_acquire)) {
        PostThreadMessageW(id, WM_NULL, 0, 0);
    }
    if (g_thread.joinable()) g_thread.join();
}

void InputGuard::setMenuOpen(bool open) {
    if (g_menuOpen.exchange(open, std::memory_order_relaxed) == open) return;

    // The detours are already gated by the line above, from this instant, on
    // every thread. Only the pointer state itself has to be marshalled.
    if (HWND hwnd = D3DHook::get().window(); hwnd && g_syncMessage) {
        PostMessageW(hwnd, g_syncMessage, open ? 1u : 0u, 0);
    }
}

bool InputGuard::menuOpen() const {
    return g_menuOpen.load(std::memory_order_relaxed);
}

void InputGuard::reassert() {
    if (!g_menuOpen.load(std::memory_order_relaxed) || !g_syncMessage) return;

    static std::atomic<ULONGLONG> lastPost{ 0 };
    const ULONGLONG now = GetTickCount64();
    ULONGLONG last = lastPost.load(std::memory_order_relaxed);
    if (now - last < 500) return;
    if (!lastPost.compare_exchange_strong(last, now, std::memory_order_relaxed)) return;

    if (HWND hwnd = D3DHook::get().window()) {
        PostMessageW(hwnd, g_syncMessage, 1u, 0);
    }
}

UINT InputGuard::syncMessage() const {
    return g_syncMessage;
}

void InputGuard::onSyncMessage(WPARAM wParam) {
    applyCursorState(wParam != 0, D3DHook::get().window());
}

} // namespace glacier::ui
