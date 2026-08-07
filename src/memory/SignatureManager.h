#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Central registry of byte-pattern signatures and struct offsets, keyed by a
// human name. This is the single place that knows anything build-specific about
// Minecraft: Bedrock — every other part of the client asks for
// "ClientInstance::update" or "Options::getGamma" by name and gets back a
// resolved address / offset. When the game updates, only the seed table moves.
//
// The seed table itself lives in Signatures.cpp, deliberately split out: it is
// the one file in the tree carrying third-party-derived data (see
// docs/acknowledgements.md). Keeping it isolated means the registry logic here
// stays clean of build-specific numbers.
namespace glacier::memory {

class SignatureManager {
public:
    static SignatureManager& get() {
        static SignatureManager instance;
        return instance;
    }

    // Seeds the table for the targeted game build. Defined in Signatures.cpp.
    void seedBedrock();

    void addSignature(std::string name, std::string idaPattern);
    void addOffset(std::string name, std::ptrdiff_t offset);

    // ── Version gating (forward-compatibility seam) ──
    // Glacier currently targets a single game build, so this is always true. It
    // exists so a per-version table (seed oldest-first, newer builds override)
    // can be introduced later by teaching *this* function about the detected
    // build — without touching a single call site in Signatures.cpp.
    [[nodiscard]] static bool checkAboveOrEqual(int /*major*/, int /*minor*/, int /*patch*/) {
        return true;
    }

    // Resolves every registered signature against the main module. Work is split
    // across hardware_concurrency() threads: each pattern is an independent full
    // scan of a ~100MB image, so this is embarrassingly parallel and cuts attach
    // time roughly linearly with core count.
    //
    // Returns the number that resolved. Every failure is logged by name — a
    // clean "signature X not found" list is the intended diagnostic when the
    // game updates, and is far more useful than a crash.
    std::size_t scanAll();

    // Names that failed to resolve in the last scanAll(), in registration order.
    [[nodiscard]] const std::vector<std::string>& unresolved() const { return m_unresolved; }

    // Resolved absolute address of a signature (0 if missing/unresolved).
    [[nodiscard]] std::uintptr_t sig(std::string_view name) const;
    // Registered offset (0 if missing).
    [[nodiscard]] std::ptrdiff_t offset(std::string_view name) const;

private:
    SignatureManager() = default;

    // Transparent hash so the maps can be queried with a std::string_view
    // without constructing a temporary std::string on every lookup.
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    struct Entry {
        std::string    pattern;
        std::uintptr_t address = 0;
    };

    std::unordered_map<std::string, Entry, StringHash, std::equal_to<>> m_sigs;
    std::unordered_map<std::string, std::ptrdiff_t, StringHash, std::equal_to<>> m_offsets;
    std::vector<std::string> m_unresolved;
    bool m_seeded = false;
};

} // namespace glacier::memory
