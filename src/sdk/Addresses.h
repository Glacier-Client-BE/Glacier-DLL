#pragma once
//
// Game-version-specific signature offsets and patterns.
//
// THIS FILE IS THE ONLY PLACE THAT NEEDS TO CHANGE WHEN THE GAME UPDATES.
// Everything else in the SDK works through resolved pointers obtained here.
//
// Patterns are byte-pattern strings (see Util::patternScan):
//   "48 8B 0D ? ? ? ? 48 85 C9 74 ? ..."
//
// Until real offsets are filled in, all resolvers below return nullptr and the
// dependent modules will short-circuit out gracefully.
//
#include <Windows.h>
#include <cstdint>
#include "../core/Util.h"

namespace Glacier::sdk {

class Addresses {
public:
    static Addresses& get();

    // Call once after the DLL is loaded into Minecraft.Windows.exe.
    void resolve();
    [[nodiscard]] bool ready() const { return ready_; }

    // Resolved pointers (nullptr until a real signature is wired).
    void* clientInstance     = nullptr; // ClientInstance**
    void* localPlayerVtable  = nullptr;
    void* sendPacketFn       = nullptr;
    void* renderLevelFn      = nullptr;
    void* keyMapFn           = nullptr;
    void* getMatrixFn        = nullptr;

private:
    bool    ready_ = false;
    HMODULE base_  = nullptr;
};

} // namespace Glacier::sdk
