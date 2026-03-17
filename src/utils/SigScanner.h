#include "SigScanner.h"
#include <sstream>
#include <iostream>

uintptr_t SigScanner::scan(const std::string& moduleName, const std::string& pattern) {
    HMODULE hModule = GetModuleHandleA(moduleName.c_str());
    if (!hModule) return 0;

    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) 
        return 0;

    uintptr_t start = (uintptr_t)modInfo.lpBaseOfDll;
    size_t size = (size_t)modInfo.SizeOfImage;

    std::vector<uint8_t> patternBytes;
    std::vector<uint8_t> maskBytes;
    parsePattern(pattern, patternBytes, maskBytes);

    if (patternBytes.empty()) return 0;

    for (uintptr_t i = 0; i < size - patternBytes.size(); ++i) {
        if (matchPattern((uint8_t*)(start + i), patternBytes.data(), maskBytes.data(), patternBytes.size())) {
            return start + i;
        }
    }

    return 0;
}

uintptr_t SigScanner::scan(const std::string& pattern) {
    return scan("Minecraft.Windows.exe", pattern);
}

uintptr_t SigScanner::resolveRip(uintptr_t instrAddr, int offsetFieldPos, int instrLen) {
    if (!instrAddr) return 0;
    int32_t relativeOffset = *(int32_t*)(instrAddr + offsetFieldPos);
    return instrAddr + instrLen + relativeOffset;
}

bool SigScanner::matchPattern(const uint8_t* data, const uint8_t* pat, const uint8_t* mask, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (mask[i] && data[i] != pat[i]) {
            return false;
        }
    }
    return true;
}

void SigScanner::parsePattern(const std::string& pattern, std::vector<uint8_t>& bytes, std::vector<uint8_t>& mask) {
    std::stringstream ss(pattern);
    std::string segment;

    while (ss >> segment) {
        if (segment == "?" || segment == "??") {
            bytes.push_back(0);
            mask.push_back(0); // 0 = Ignore
        } else {
            bytes.push_back((uint8_t)std::stoul(segment, nullptr, 16));
            mask.push_back(1); // 1 = Match
        }
    }
}