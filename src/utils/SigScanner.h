#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <Windows.h>
#include <Psapi.h>

class SigScanner {
public:
    static uintptr_t scan(const std::string& moduleName, const std::string& pattern);
    static uintptr_t scan(const std::string& pattern);
    static uintptr_t resolveRip(uintptr_t instrAddr, int offsetFieldPos = 3, int instrLen = 7);

private:
    static bool matchPattern(const uint8_t* data, const uint8_t* pat, const uint8_t* mask, size_t len);
    static void parsePattern(const std::string& pattern,
                             std::vector<uint8_t>& bytes,
                             std::vector<uint8_t>& mask);
};