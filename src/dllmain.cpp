// ────────────────────────────────────────────────────────────────────────────
//  Glacier DLL — internal client entry point
//
//  Injected into Minecraft: Bedrock Edition (Minecraft.Windows.exe). DllMain is
//  kept minimal: it does nothing but spin up a dedicated initialization thread,
//  because almost everything the client needs to do (allocating a console,
//  creating a D3D device for vtable discovery, installing hooks) is illegal to
//  do while holding the loader lock.
// ────────────────────────────────────────────────────────────────────────────

#include <Windows.h>

#include "Glacier.h"

namespace {

DWORD WINAPI initThread(LPVOID module) {
    // Never returns normally: start() runs the client's logic loop and then
    // calls FreeLibraryAndExitThread to unload cleanly.
    glacier::Glacier::get().start(static_cast<HMODULE>(module));
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            // Don't get DLL_THREAD_* callbacks — we never need them and they add
            // overhead to every thread the game spawns.
            DisableThreadLibraryCalls(module);

            // Hand off to a worker thread. We deliberately do NOT block here —
            // everything below CreateThread must run outside the loader lock.
            const HANDLE h = CreateThread(nullptr, 0, &initThread, module, 0, nullptr);
            if (h) CloseHandle(h);
            break;
        }

        case DLL_PROCESS_DETACH:
            // Process teardown: the logic loop's own cleanup path already ran if
            // the user unloaded with END. Nothing safe to do here otherwise.
            break;

        default:
            break;
    }
    return TRUE;
}
