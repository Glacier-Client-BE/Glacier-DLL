#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

// Confirms melee hits via the game's own ActorEventPacket(HURT_ANIMATION) —
// the same signal Latite's Combo Counter uses instead of trusting that a
// swing landed just because it was thrown (see reference/latite's
// ComboCounter.cpp: it listens for a PacketReceiveEvent and only increments
// on a matching HURT_ANIMATION, not on the attack call itself).
//
// One thing Latite's version does that this one does NOT: match the hurt
// animation's runtimeID against the entity that was actually attacked.
// Actor::getRuntimeID() has no discoverable signature, offset, or vtable
// index anywhere in the reference sources — not a risk that was weighed and
// accepted like the ones below, just genuinely unavailable. Without it there
// is no way to convert "the Actor* GameSDK::recordAttack saw" into the
// runtime ID the packet carries, so this reports "some actor took a hurt
// animation shortly after I swung" rather than "the thing I hit was hurt".
// See docs/HANDOFF.md for what closing that gap would need.
//
// Everything else here IS the real mechanism, with two hand-derived pieces
// documented in HitConfirmation.cpp:
//   - Packet::handler and ActorEventPacket::eventID's byte offsets, computed
//     from Packet.h's declared layout rather than a CLASS_FIELD the sync
//     tool could extract — Latite hand-declares these fields directly.
//   - The ABI for calling MinecraftPackets::createPacket, which returns
//     std::shared_ptr<Packet> by value (a hidden caller-owned return slot,
//     the standard MSVC x64 convention for any type with a non-trivial
//     destructor).
// Both are read only under SEH guards; a wrong value degrades this feature,
// it does not crash the game.
namespace glacier::sdk {

class HitConfirmation {
public:
    static HitConfirmation& get() {
        static HitConfirmation instance;
        return instance;
    }

    // Off by default, overridable with `hitConfirmation = true` under
    // [Glacier] in config.ini. Unlike ItemRendering — whose worst case is
    // cross-checked and caught before it can corrupt anything — the two
    // hand-derived offsets here have no independent second source to compare
    // against, so this stays opt-in until someone has run it against a live
    // client. Must be called before installHooks() to have any effect.
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

    // Resolves MinecraftPackets::createPacket, bootstraps a throwaway
    // ActorEventPacket to read its dispatcher vtable, and swaps that
    // vtable's slot 1. Safe to call when disabled or when the signature is
    // missing — it logs what happened and everything else in this class
    // becomes inert.
    void installHooks();

    bool available() const { return m_available.load(std::memory_order_relaxed); }

    // How long ago a HURT_ANIMATION was last observed, for ANY actor —
    // nullopt if none has been seen this session. Combo Counter narrows this
    // to "shortly after I swung", which is the closest approximation of
    // "I hit something" available without the runtime-ID match above.
    std::optional<std::uint64_t> lastHurtAnimationAge() const;

    // Called by the hook.
    void recordHurtAnimation();

private:
    HitConfirmation() = default;

    bool                       m_enabled = false;
    std::atomic<bool>          m_available{ false };
    std::atomic<std::uint64_t> m_stamp{ 0 };
};

} // namespace glacier::sdk
