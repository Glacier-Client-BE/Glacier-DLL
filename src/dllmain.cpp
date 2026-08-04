// ────────────────────────────────────────────────────────────────────────────
//  Glacier DLL — internal client entry point
//
//  Injected into Minecraft: Bedrock Edition (Minecraft.Windows.exe). DllMain is
//  kept minimal: it does nothing but spin up a dedicated initialization thread,
//  because almost everything the client needs to do (allocating a console,
//  creating a D3D device for vtable discovery, installing hooks) is illegal to
//  do while holding the loader lock.
//
//  This is currently a scaffold build: the hook/signature/module/UI subsystems
//  land in later phases (see the project plan). For now the init thread only
//  proves the DLL loads, logs, and unloads cleanly.
// ────────────────────────────────────────────────────────────────────────────

#include <Windows.h>

#include "util/Logger.h"

namespace {

DWORD WINAPI initThread(LPVOID /*module*/) {
    using namespace glacier;

    Logger::get().attachConsole();
    LOG_INFO("Glacier attaching (build " __DATE__ " " __TIME__ ")");
    LOG_INFO("Scaffold build — no hooks installed yet");

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
            glacier::Logger::get().detachConsole();
            break;

        default:
            break;
    }
    return TRUE;
}
