#include "Memory.h"

#include <Psapi.h>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "Psapi.lib")

namespace glacier::memory {

namespace {

// Parses "48 8B ? E8" -> bytes {0x48,0x8B,...} + wildcard mask.
struct ParsedPattern {
    std::vector<std::uint8_t> bytes;
    std::vector<bool>         wildcard;
};

ParsedPattern parse(std::string_view ida) {
    ParsedPattern out;
    std::size_t i = 0;
    while (i < ida.size()) {
        const char c = ida[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (c == '?') {
            out.bytes.push_back(0);
            out.wildcard.push_back(true);
            // consume "??" or "?"
            while (i < ida.size() && ida[i] == '?') ++i;
            continue;
        }
        // two hex nibbles
        const auto hex = [](char ch) -> int {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            return -1;
        };
        const int hi = hex(ida[i]);
        const int lo = (i + 1 < ida.size()) ? hex(ida[i + 1]) : -1;
        if (hi >= 0 && lo >= 0) {
            out.bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
            out.wildcard.push_back(false);
            i += 2;
        } else {
            ++i;
        }
    }
    return out;
}

} // namespace

ModuleRange moduleRange(const wchar_t* moduleName) {
    const HMODULE mod = GetModuleHandleW(moduleName);
    if (!mod) return {};

    MODULEINFO info{};
    if (!GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info))) {
        return {};
    }
    return ModuleRange{ reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll), info.SizeOfImage };
}

std::optional<std::uintptr_t> findSignature(std::string_view ida, const ModuleRange& range) {
    if (range.base == 0 || range.size == 0) return std::nullopt;

    const ParsedPattern pat = parse(ida);
    if (pat.bytes.empty()) return std::nullopt;

    const auto* const start = reinterpret_cast<const std::uint8_t*>(range.base);
    const std::size_t scanLen = range.size - pat.bytes.size();

    for (std::size_t off = 0; off <= scanLen; ++off) {
        const std::uint8_t* const here = start + off;
        bool matched = true;
        for (std::size_t k = 0; k < pat.bytes.size(); ++k) {
            if (!pat.wildcard[k] && here[k] != pat.bytes[k]) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return reinterpret_cast<std::uintptr_t>(here);
        }
    }
    return std::nullopt;
}

std::optional<std::uintptr_t> findSignature(std::string_view ida) {
    return findSignature(ida, moduleRange());
}

std::uintptr_t resolveRelative(std::uintptr_t addressOfDisp, int dispOffset, int instructionLength) {
    const auto disp = *reinterpret_cast<const std::int32_t*>(addressOfDisp + dispOffset);
    return addressOfDisp + dispOffset + sizeof(std::int32_t) + instructionLength + disp;
}

std::uintptr_t followCall(std::uintptr_t address) {
    // E8 <rel32>  ->  next_instr + rel32
    const auto rel = *reinterpret_cast<const std::int32_t*>(address + 1);
    return address + 5 + rel;
}

bool isExecutable(std::uintptr_t address) {
    if (!address) return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) {
        return false;
    }
    if (mbi.State != MEM_COMMIT) return false;

    constexpr DWORD kExecutable = PAGE_EXECUTE | PAGE_EXECUTE_READ
                                | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if ((mbi.Protect & kExecutable) == 0) return false;

    // Alignment padding between functions. Landing here means the displacement
    // was decoded from the wrong instruction — the page is right, the address
    // is not.
    const auto first = *reinterpret_cast<const std::uint8_t*>(address);
    return first != 0xCC && first != 0x00;
}

} // namespace glacier::memory
