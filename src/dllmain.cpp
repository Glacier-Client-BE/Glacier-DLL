#include "Client.h"
#include <Windows.h>

DWORD WINAPI clientThread(LPVOID lpParam) {
    Client::get().init(reinterpret_cast<HMODULE>(lpParam));
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0, clientThread, hModule, 0, nullptr));
    }
    return TRUE;
}
