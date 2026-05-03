#include "Addresses.h"
#include "../core/Logger.h"

namespace Glacier::sdk {

Addresses& Addresses::get() {
    static Addresses A;
    return A;
}

void Addresses::resolve() {
    base_ = GetModuleHandleW(nullptr); // the host EXE (Minecraft.Windows.exe)
    if (!base_) {
        Logger::get().warn("SDK", "Failed to obtain host module handle");
        return;
    }

    // ---- Sigs go here. -----------------------------------------------------
    // Each line is a TODO that needs filling in for a specific game version.
    // The patterns below are placeholders showing the intended shape.
    //
    // clientInstance = (void*)Util::patternScan(base_, "48 8B 0D ? ? ? ? 48 85 C9 ?? ?? 48 8B");
    // sendPacketFn   = (void*)Util::patternScan(base_, "48 89 5C 24 ?? 48 89 6C 24 ?? 56 57 41 54 41 56 41 57");
    // ...

    Logger::get().info("SDK", "Addresses::resolve() complete (offsets are stubs - fill Addresses.cpp)");
    ready_ = true;
}

} // namespace Glacier::sdk
