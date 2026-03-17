#include "SigScanner.h"
#include "Logger.h"
#include <vector>
#include <sstream>
#include <iomanip>

// ── Pattern parser ────────────────────────────────────────────────────────────
// Changed std::vector<bool> to std::vector<uint8_t> to fix .data() access
void SigScanner::parsePattern(const std::string& pattern,
                               std::vector<uint8_t>& bytes,
                               std::vector<uint8_t>& mask)
{
    std::istringstream ss(pattern);
    std::string token;
    while (ss >> token) {
        if (token == "??" || token == "?") {
            bytes.push_back(0x00);
            mask.push_back(0); // wildcard (false)
        } else {
            bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
            mask.push_back(1);  // must match (true)
        }
    }
}

// Changed const bool* to const uint8_t* to match the vector change
bool SigScanner::matchPattern(const uint8_t* data, const uint8_t* pat,
                               const uint8_t* mask, size_t len)
{
    for (size_t i = 0; i < len; i++)
        if (mask[i] && data[i] != pat[i]) return false;
    return true;
}

// ── Core scan ─────────────────────────────────────────────────────────────────
uintptr_t SigScanner::scan(const std::string& moduleName, const std::string& pattern) {
    HMODULE hMod = GetModuleHandleA(moduleName.c_str());
    if (!hMod) {
        // Note: Make sure Logger::warn supports this syntax or use Logger::warn("msg" + moduleName);
        Logger::warn("SigScanner: module not found"); 
        return 0;
    }

    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hMod);
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uintptr_t>(hMod) + dosHeader->e_lfanew);

    uintptr_t base    = reinterpret_cast<uintptr_t>(hMod);
    uintptr_t textStart = 0;
    size_t    textSize  = 0;

    // Walk section headers to find .text
    auto* section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, section++) {
        if (memcmp(section->Name, ".text", 5) == 0) {
            textStart = base + section->VirtualAddress;
            textSize  = section->Misc.VirtualSize;
            break;
        }
    }

    if (textStart == 0) {
        // Fallback: scan entire module image
        textStart = base;
        textSize  = ntHeaders->OptionalHeader.SizeOfImage;
    }

    std::vector<uint8_t> patBytes;
    std::vector<uint8_t> patMask; // Changed from bool to uint8_t
    parsePattern(pattern, patBytes, patMask);

    size_t patLen = patBytes.size();
    if (patLen == 0) return 0;

    const uint8_t* start = reinterpret_cast<const uint8_t*>(textStart);
    for (size_t i = 0; i + patLen <= textSize; i++) {
        // .data() now works because patMask is a vector of uint8_t
        if (matchPattern(start + i, patBytes.data(), patMask.data(), patLen))
            return textStart + i;
    }

    Logger::warn("SigScanner: pattern not found");
    return 0;
}

uintptr_t SigScanner::scan(const std::string& pattern) {
    return scan("Minecraft.Windows.exe", pattern);
}

// ── RIP resolver ──────────────────────────────────────────────────────────────
uintptr_t SigScanner::resolveRip(uintptr_t instrAddr, int offsetFieldPos, int instrLen) {
    if (!instrAddr) return 0;
    int32_t rel = *reinterpret_cast<const int32_t*>(instrAddr + offsetFieldPos);
    return instrAddr + instrLen + rel;
}