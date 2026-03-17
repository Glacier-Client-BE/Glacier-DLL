#pragma once
#include <cstdint>
#include <string>
#include <Windows.h>

// ─────────────────────────────────────────────────────────────────────────────
//  SigScanner — scans loaded module memory for byte patterns (IDA-style).
//  Usage:
//      uintptr_t addr = SigScanner::scan("Minecraft.Windows.exe",
//                           "48 8B 05 ?? ?? ?? ?? 48 85 C0");
//      if (addr) ClientInstance::instancePtr() = *(ClientInstance**)(addr + 3);
// ─────────────────────────────────────────────────────────────────────────────
class SigScanner {
public:
    // Scans the .text section of `moduleName` for `pattern`.
    // Returns the absolute address of the first match, or 0 on failure.
    static uintptr_t scan(const std::string& moduleName, const std::string& pattern);

    // Scan the current process's default module (Minecraft.Windows.exe)
    static uintptr_t scan(const std::string& pattern);

    // Resolve a relative 32-bit offset at `addr+offset` into an absolute address.
    // Useful for: mov rax,[rip+xx] instructions where offset field starts at +3.
    static uintptr_t resolveRip(uintptr_t instrAddr, int offsetFieldPos = 3, int instrLen = 7);

private:
    static bool matchPattern(const uint8_t* data, const uint8_t* pat, const bool* mask, size_t len);
    static void parsePattern(const std::string& pattern,
                             std::vector<uint8_t>& bytes,
                             std::vector<bool>&    mask);
};
