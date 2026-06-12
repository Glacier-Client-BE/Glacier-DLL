#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

// Central registry of byte-pattern signatures and struct offsets, keyed by a
// human name. This is the single place that knows anything build-specific about
// Minecraft: Bedrock — every other part of the client asks for "ClientInstance::
// update" or "ClientInstance::minecraftGame" by name and gets back a resolved
// address / offset. When the game updates, only the seed table below changes.
//
// Architecture adapted from Flarial's SignatureAndOffsetManager (`Mgr`): one
// registry, version-specific seed functions, scan-once. The seeded values are
// the real signatures/offsets reverse-engineered for Bedrock 1.21.13x.
namespace glacier::memory {

class SignatureManager {
public:
    static SignatureManager& get() {
        static SignatureManager instance;
        return instance;
    }

    // Seeds the table with the signatures/offsets for the targeted game build.
    void seedBedrock();

    void addSignature(std::string name, std::string idaPattern);
    void addOffset(std::string name, std::ptrdiff_t offset);

    // Resolves every registered signature against the main module in one pass.
    // Returns the number that resolved successfully.
    std::size_t scanAll();

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
};

} // namespace glacier::memory
