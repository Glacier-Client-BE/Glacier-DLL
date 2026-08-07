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

    // `derefOffset` says how to turn the byte-pattern match into the address the
    // caller actually wants:
    //
    //   kMatchIsTarget (default)  the pattern sits at the start of the thing —
    //                             use the match address as-is.
    //   >= 0                      the pattern matches an *instruction that
    //                             references* the target (typically a `call
    //                             rel32`, sometimes a `lea`/`mov` RIP-relative).
    //                             At match+derefOffset there is a signed 32-bit
    //                             displacement; the target is
    //                             match + derefOffset + 4 + disp.
    //
    // The second form exists because some functions have no stable prologue to
    // match on, so upstream matches a call site instead. Getting this wrong
    // yields an address that looks plausible and executes garbage, which is why
    // it is stated per entry in the generated table rather than inferred.
    static constexpr int kMatchIsTarget = -1;

    // What a deref'd signature points AT. This is not cosmetic: a resolved
    // address is validated against it, and the two kinds live in different
    // sections with different page protection.
    //
    //   Code  a function — to hook or to call. Must be executable.
    //   Data  a global variable — to read. Must NOT be executable, because a
    //         global that resolves into .text means the displacement was
    //         decoded from the wrong instruction.
    //
    // Only meaningful with a deref offset; a pattern that matched directly is
    // by construction sitting on the bytes it matched.
    enum class TargetKind { Code, Data };

    void addSignature(std::string name, std::string idaPattern,
                      int derefOffset = kMatchIsTarget,
                      TargetKind kind = TargetKind::Code);
    void addOffset(std::string name, std::ptrdiff_t offset);

    // ── Version gating ──
    // Answers "is the attached game at least this build?" against the version
    // read from the executable itself. Seed the table oldest-first and guard
    // newer overrides with this, so one table serves multiple builds:
    //
    //     addOffset("MinecraftGame::gameRenderer", 0xD30);          // 1.21.13x
    //     if (checkAboveOrEqual(1, 26, 0))
    //         addOffset("MinecraftGame::gameRenderer", 0xD70);      // 1.26+
    //
    // Falls back to true when the version can't be read, so an unreadable
    // version resource degrades to "assume newest" rather than silently
    // seeding an ancient table.
    [[nodiscard]] static bool checkAboveOrEqual(int major, int minor, int patch);

    // The build this signature table was written for. Compared against the
    // detected version at attach so a mismatch is stated, not discovered.
    static constexpr int kTargetMajor = 1;
    static constexpr int kTargetMinor = 26;
    static constexpr int kTargetPatch = 40;

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
        int            derefOffset = kMatchIsTarget;
        TargetKind     kind = TargetKind::Code;
        std::uintptr_t address = 0;
    };

    std::unordered_map<std::string, Entry, StringHash, std::equal_to<>> m_sigs;
    std::unordered_map<std::string, std::ptrdiff_t, StringHash, std::equal_to<>> m_offsets;
    std::vector<std::string> m_unresolved;
    bool m_seeded = false;
};

} // namespace glacier::memory
