#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include <Windows.h>

// Pattern (signature) scanning + module range helpers. Bedrock ships without
// exported symbols for the internals we hook, so every SDK address is resolved
// at runtime from an IDA-style byte pattern ("48 8B ?? E8 ? ? ? ?"). This keeps
// Glacier resilient across Minecraft updates: only the signatures move, not the
// architecture.
namespace glacier::memory {

struct ModuleRange {
    std::uintptr_t base = 0;
    std::size_t    size = 0;

    [[nodiscard]] bool contains(std::uintptr_t addr) const noexcept {
        return addr >= base && addr < base + size;
    }
};

// Returns the in-memory range of a loaded module ("Minecraft.Windows.exe" when
// passed nullptr -> the main executable).
ModuleRange moduleRange(const wchar_t* moduleName = nullptr);

// Scans `range` for the first match of an IDA-style signature. "??" / "?" are
// wildcards. Returns the absolute address of the match, or nullopt.
std::optional<std::uintptr_t> findSignature(std::string_view ida, const ModuleRange& range);

// Convenience overload that scans the main module.
std::optional<std::uintptr_t> findSignature(std::string_view ida);

// Resolves a RIP-relative reference (e.g. the target of a `lea`/`call`) given
// the address of the instruction's displacement and the size of the trailing
// operand bytes after the displacement.
std::uintptr_t resolveRelative(std::uintptr_t addressOfDisp, int dispOffset = 0, int instructionLength = 0);

// Follows a relative CALL/JMP at `address` to its destination.
std::uintptr_t followCall(std::uintptr_t address);

// Resolves a RIP-relative reference the way reverse-engineering tools express
// it: at `sig + offset` there's a 4-byte signed displacement, and the absolute
// target is `sig + offset + 4 + disp`. (Adapted from Flarial's offsetFromSig.)
inline std::uintptr_t offsetFromSig(std::uintptr_t sig, int offset) {
    if (sig == 0) return 0;
    return sig + offset + 4 + *reinterpret_cast<const std::int32_t*>(sig + offset);
}

// Reads/writes a field at a byte offset from a base object pointer — the core
// primitive for offset-based SDK access. (Adapted from Flarial's direct_access.)
template <typename T>
T& memberAt(void* base, std::ptrdiff_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(base) + offset);
}

// Calls a virtual function at a runtime vtable index on `thisptr`. Bedrock
// internals are reached almost entirely through virtual dispatch, and the index
// itself is usually decoded from a signature. (Adapted from Memory::CallVFuncI.)
template <typename Ret, typename... Args>
Ret callVirtualI(std::uint32_t index, void* thisptr, Args... args) {
    using Fn = Ret(__thiscall*)(void*, Args...);
    return (*static_cast<Fn**>(thisptr))[index](thisptr, args...);
}

// Walks a multi-level pointer chain, dereferencing then adding each offset.
inline std::uintptr_t followChain(std::uintptr_t base, std::initializer_list<std::ptrdiff_t> offsets) {
    std::uintptr_t addr = base;
    for (auto off : offsets) {
        if (!addr) return 0;
        addr = *reinterpret_cast<std::uintptr_t*>(addr);
        addr += off;
    }
    return addr;
}

// RAII page-protection change that restores on scope exit + flushes the
// instruction cache — for safe byte patching. (Adapted from ScopedVirtualProtect.)
class ScopedProtect {
public:
    ScopedProtect(void* addr, std::size_t size, DWORD newProtect)
        : m_addr(addr), m_size(size) {
        m_ok = VirtualProtect(addr, size, newProtect, &m_old) != 0;
    }
    ~ScopedProtect() {
        if (m_ok) {
            VirtualProtect(m_addr, m_size, m_old, &m_old);
            FlushInstructionCache(GetCurrentProcess(), m_addr, m_size);
        }
    }
    ScopedProtect(const ScopedProtect&) = delete;
    ScopedProtect& operator=(const ScopedProtect&) = delete;

private:
    void*       m_addr;
    std::size_t m_size;
    DWORD       m_old = 0;
    bool        m_ok  = false;
};

} // namespace glacier::memory
