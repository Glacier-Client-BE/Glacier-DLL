#pragma once

#include <Windows.h>

// Hands input and the mouse pointer between the game and Glacier's menu.
//
// ── Why the previous design could not work ──
//
// Two independent mistakes, both invisible from the code and both proven by
// the same symptom ("the menu opens, the mouse turns my head, and I can't
// click anything"):
//
//  1. **Bedrock reads mouse look through Raw Input, and a WH_MOUSE_LL hook
//     does not intercept Raw Input.** Low-level hooks sit on the message
//     path. RIM_TYPEMOUSE deltas are delivered separately and never pass
//     through them, so swallowing WM_MOUSEMOVE stopped the OS *pointer*
//     while the game's *camera* carried on reading the real motion. The
//     keyboard half of the same hook did work — that asymmetry is exactly
//     what a Raw-Input mouse and a message-queue keyboard look like.
//
//  2. **Cursor state is per-thread, and none of it was being set on the
//     window's thread.** SetCapture, ReleaseCapture, ShowCursor and SetCursor
//     all act on the calling thread's input state. They were being called
//     from the Present hook (the game's render thread) and from Glacier's own
//     logic thread, where they are silently no-ops for the game's window.
//     That is why a log could report the cursor "released (ok)" with nothing
//     whatsoever having changed on screen.
//
// ── What replaces it ──
//
//  * **Camera look and clicks** are stopped at the source: GetRawInputData
//    and GetRawInputBuffer are hooked, and while the menu is open every
//    RIM_TYPEMOUSE record they hand back has its deltas and button flags
//    zeroed. The game still receives its input events and keeps its internal
//    state consistent; the events just say the mouse did not move. This is
//    the API-level equivalent of what Flarial does one layer lower by
//    hooking MouseDevice::feed / InputHandler::tick (see
//    reference/flarial/src/Client/Hook/Hooks/Input/MouseHook.cpp) — same
//    effect, no game signature to break on the next update.
//
//  * **The pointer** is taken away from the game by hooking ClipCursor,
//    SetCursorPos, SetCapture, ShowCursor and SetCursor, so the game cannot
//    re-hide or re-clip it on the next frame — Flarial hooks ClipCursor for
//    precisely this reason (ClipCursorHook.hpp). What the game asked for is
//    remembered and handed back when the menu closes.
//
//  * **All of that is applied on the window thread**, by posting a private
//    message to the game window and doing the work inside Glacier's WndProc.
//    See onSyncMessage.
//
//  * **Movement keys** keep the WH_KEYBOARD_LL hook, which is confirmed
//    working in-game; the mouse half of that hook is gone.
namespace glacier::ui {

class InputGuard {
public:
    static InputGuard& get() {
        static InputGuard instance;
        return instance;
    }

    // Installs the low-level keyboard hook on a dedicated pump thread. Call
    // once during Glacier::start(). Safe to call again if already running.
    void start();

    // Hooks the Win32 pointer and Raw Input entry points. Must come after
    // HookManager::initialize(); the hooks are owned by the HookManager and
    // removed by its shutdown().
    bool installApiHooks();

    // Stops the pump thread and stops this object using its trampolines.
    // Must run BEFORE HookManager::shutdown(), which frees them.
    void stop();

    // The single switch. Safe from any thread: it flips the atomic the
    // detours read, then posts the cursor work to the window thread.
    void setMenuOpen(bool open);
    bool menuOpen() const;

    // Re-posts the cursor sync at most a few times a second while the menu is
    // open. Cheap self-healing: if anything ever does get the pointer back
    // (an alt-tab, a screen the game pushes on its own), the menu recovers
    // without the user having to close and reopen it.
    void reassert();

    // ── Window thread only ──

    // Private message used to marshal cursor work onto the window thread.
    // Registered, not a WM_APP constant, so it cannot collide with anything
    // the game or another injected DLL uses.
    UINT syncMessage() const;

    // Handles that message. Called from Glacier::wndProc, which is the whole
    // point: that is the one place Glacier runs on the window's own thread.
    void onSyncMessage(WPARAM wParam);

private:
    InputGuard() = default;
};

} // namespace glacier::ui
