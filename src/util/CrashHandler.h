#pragma once

// Turns "it crashed" into an address.
//
// Glacier reaches into another process's internals through imported offsets and
// pattern-matched function addresses. When one of those is wrong for a build,
// the result is an access violation somewhere inside the game — and from the
// outside every one of those looks identical: the game closes and the log ends
// wherever it happened to be. Diagnosing that by reasoning about which change
// came last costs a full test cycle per guess.
//
// This logs the faulting address, attributes it to a module (Glacier.dll or the
// game) with an offset that can be looked up in a map or disassembler, and says
// what Glacier was doing at the time. It never suppresses the crash: the
// handler logs and passes the exception on.
namespace glacier {

class CrashHandler {
public:
    static void install();
    static void remove();

    // Names the operation currently in flight, for the crash log. Cheap enough
    // to set on any path that calls into game code — a plain pointer store to a
    // thread-local, no allocation, no lock.
    //
    // The string must outlive the scope, so only ever pass a literal.
    class Scope {
    public:
        explicit Scope(const char* what) : m_previous(s_activity) { s_activity = what; }
        ~Scope() { s_activity = m_previous; }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        const char* m_previous;
    };

    static const char* activity() { return s_activity ? s_activity : "(idle)"; }

private:
    static inline thread_local const char* s_activity = nullptr;
    static inline void* s_handle = nullptr;
};

} // namespace glacier

// GLACIER_ACTIVITY("walking the ClientInstance map");
//
// Two levels of indirection so __LINE__ expands before pasting — without it
// every use makes the same identifier and two in one scope collide.
#define GLACIER_CONCAT_INNER(a, b) a##b
#define GLACIER_CONCAT(a, b) GLACIER_CONCAT_INNER(a, b)
#define GLACIER_ACTIVITY(what) \
    ::glacier::CrashHandler::Scope GLACIER_CONCAT(glacierActivity_, __LINE__) { what }
