#include "CrashHandler.h"

#include "Logger.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <Windows.h>

namespace glacier {

namespace {

// First-chance handlers see every exception, including ones the game handles
// perfectly well on its own and the ones Glacier's own __try blocks are there
// to absorb. Logging all of them would bury the useful line, so this reports a
// small number and then goes quiet.
constexpr int kMaxReports = 5;
std::atomic<int> s_reported{ 0 };

// Resolves an address to "module+offset", which is the form that can actually
// be looked up later. Bare absolute addresses are useless across runs because
// of ASLR.
void describeAddress(std::uintptr_t address, char* out, std::size_t outSize) {
    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(address), &module) && module) {
        // GetModuleFileNameA, not psapi's GetModuleBaseNameA: it needs only the
        // HMODULE we already hold. The psapi call was silently failing here and
        // sending every faulting address — including ordinary system DLLs — down
        // the "(no module)" path, which made a perfectly attributable crash look
        // like it came from nowhere.
        char path[MAX_PATH]{};
        if (GetModuleFileNameA(module, path, sizeof(path)) > 0) {
            const char* name = std::strrchr(path, '\\');
            name = name ? name + 1 : path;
            const auto base = reinterpret_cast<std::uintptr_t>(module);
            _snprintf_s(out, outSize, _TRUNCATE, "%s+0x%llX",
                        name, static_cast<unsigned long long>(address - base));
            return;
        }
    }

    // Genuinely outside every module. Say what kind of memory it is instead:
    // MEM_PRIVATE executable memory is a hook trampoline or JIT, which is a
    // completely different investigation from a stray address.
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi)) {
        const char* kind = mbi.Type == MEM_IMAGE   ? "image"
                         : mbi.Type == MEM_MAPPED  ? "mapped"
                         : mbi.Type == MEM_PRIVATE ? "private (trampoline or JIT)"
                                                   : "unknown";
        _snprintf_s(out, outSize, _TRUNCATE,
                    "0x%llX (no module; %s, alloc base 0x%llX, protect 0x%lX)",
                    static_cast<unsigned long long>(address), kind,
                    static_cast<unsigned long long>(
                        reinterpret_cast<std::uintptr_t>(mbi.AllocationBase)),
                    static_cast<unsigned long>(mbi.Protect));
        return;
    }

    _snprintf_s(out, outSize, _TRUNCATE, "0x%llX (unmapped)",
                static_cast<unsigned long long>(address));
}

const char* exceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      return "access violation";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction";
        case EXCEPTION_PRIV_INSTRUCTION:      return "privileged instruction";
        case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer divide by zero";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned access";
        default:                              return nullptr;
    }
}

LONG CALLBACK onException(EXCEPTION_POINTERS* info) {
    const auto* record = info ? info->ExceptionRecord : nullptr;
    if (!record) return EXCEPTION_CONTINUE_SEARCH;

    // Only the fatal-looking kinds. C++ exceptions, breakpoints and the
    // debugger's thread-name notification are all normal traffic here.
    const char* name = exceptionName(record->ExceptionCode);
    if (!name) return EXCEPTION_CONTINUE_SEARCH;

    const int n = s_reported.fetch_add(1, std::memory_order_relaxed);
    if (n >= kMaxReports) return EXCEPTION_CONTINUE_SEARCH;

    char where[MAX_PATH + 32]{};
    describeAddress(reinterpret_cast<std::uintptr_t>(record->ExceptionAddress),
                    where, sizeof(where));

    LOG_ERROR("*** {} at {} — Glacier was: {}", name, where, CrashHandler::activity());

    // For an access violation the operand address says whether this was a null
    // dereference (a missing check) or a wild pointer (a wrong offset), which
    // are very different bugs.
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        && record->NumberParameters >= 2) {
        const auto op = static_cast<unsigned long long>(record->ExceptionInformation[1]);
        LOG_ERROR("    {} address 0x{:X}{}",
                  record->ExceptionInformation[0] ? "writing" : "reading", op,
                  op < 0x10000 ? " (near-null: a missing null check)"
                               : " (wild pointer: likely a wrong offset or signature)");
    }

    if (n + 1 == kMaxReports) {
        LOG_ERROR("    (further exceptions will not be reported)");
    }

    // Never swallow it. Some of these are first-chance and handled downstream;
    // the ones that aren't should crash exactly as they would have.
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void CrashHandler::install() {
    if (s_handle) return;
    // First in the chain, so the log line is written before anything else gets
    // a chance to handle, rewrite, or terminate on the exception.
    s_handle = AddVectoredExceptionHandler(1, &onException);
    if (!s_handle) {
        LOG_WARN("could not install the crash handler — a crash will not be attributed");
    }
}

void CrashHandler::remove() {
    if (!s_handle) return;
    RemoveVectoredExceptionHandler(s_handle);
    s_handle = nullptr;
}

} // namespace glacier
