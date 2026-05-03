#include "Client.h"
#include <Windows.h>

static HMODULE g_self = nullptr;

static DWORD WINAPI bootstrap(LPVOID) {
    Glacier::Client::get().start();
    return 0;
}

static DWORD WINAPI shutdown(LPVOID) {
    Glacier::Client::get().stop();
    if (g_self) FreeLibraryAndExitThread(g_self, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            g_self = hModule;
            CreateThread(nullptr, 0, bootstrap, nullptr, 0, nullptr);
            break;
        case DLL_PROCESS_DETACH:
            // Skip cleanup on process termination - loader lock is held and
            // most subsystems are already torn down. Only run stop() when the
            // DLL is unloaded explicitly (lpReserved == NULL).
            if (lpReserved == nullptr) Glacier::Client::get().stop();
            break;
    }
    return TRUE;
}

// Optional manual eject hook used by injectors that want a clean teardown.
extern "C" __declspec(dllexport) void GlacierEject() {
    CreateThread(nullptr, 0, shutdown, nullptr, 0, nullptr);
}
