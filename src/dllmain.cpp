// ────────────────────────────────────────────────────────────────────────────
//  Glacier DLL — internal client entry point
//
//  Injected into Minecraft: Bedrock Edition (Minecraft.Windows.exe). DllMain is
//  kept minimal: it does nothing but spin up a dedicated initialization thread,
//  because almost everything the client needs to do (LoadLibrary side effects,
//  creating a D3D device for vtable discovery, allocating a console) is illegal
//  to do while holding the loader lock. All real work happens on Glacier::start.
// ────────────────────────────────────────────────────────────────────────────

#include <Windows.h>

#include "Glacier.h"

namespace {

// Thread proc — owns the entire client lifetime. Returns (and frees the module)
// only after Glacier::start completes its teardown.
DWORD WINAPI initThread(LPVOID param) {
    glacier::Glacier::get().start(static_cast<HMODULE>(param));
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            // Don't get DLL_THREAD_* callbacks — we never need them and they add
            // overhead to every thread the game spawns.
            DisableThreadLibraryCalls(module);

            // Hand off to a worker thread. We deliberately do NOT block here.
            const HANDLE h = CreateThread(nullptr, 0, &initThread, module, 0, nullptr);
            if (h) CloseHandle(h);
            break;
        }

        case DLL_PROCESS_DETACH:
            // If we're being unloaded out from under ourselves (process exit or a
            // forced FreeLibrary), make sure the client stops touching game memory.
            glacier::Glacier::get().requestShutdown();
            break;

        default:
            break;
    }
    return TRUE;
}
