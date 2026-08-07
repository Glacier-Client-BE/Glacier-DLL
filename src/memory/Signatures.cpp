// ─────────────────────────────────────────────────────────────────────────────
//  THIRD-PARTY-DERIVED DATA — READ docs/acknowledgements.md BEFORE EDITING
//
//  The AOB byte patterns and struct offsets in this file are derived from the
//  open reverse-engineering work published in two other Bedrock clients:
//
//      Flarial (dll-oss)  https://github.com/flarialmc/dll-oss   — AGPLv3
//      Latite             https://github.com/LatiteClient/Latite — GPLv3
//
//  They were NOT independently derived. This single file is the reason Glacier
//  is licensed AGPLv3 rather than permissively; AGPLv3 absorbs both upstreams
//  cleanly (GPLv3 material may be incorporated under GPLv3 §13).
//
//  Two rules follow from that, and they are the whole point of this file
//  existing separately from SignatureManager.cpp:
//
//    1. Keep imported data HERE. Never inline a pattern or a struct offset
//       anywhere else in the tree — everything build-specific is looked up
//       through SignatureManager by name. If this file is ever replaced with
//       independently derived data, that must be the only change required to
//       clear the license constraint.
//
//    2. Everything else in Glacier (the scanner, the registry, the hooks, the
//       module system, the UI) is Glacier's own code and carries no such
//       constraint.
//
//  ── TARGET BUILD: Minecraft: Bedrock Edition 1.26.40 ──
//
//  Declared in SignatureManager::kTarget{Major,Minor,Patch}; the attached
//  game's real version is read from its executable at startup and compared,
//  so a mismatch is logged rather than discovered through weird behaviour.
//
//  Honest provenance: these patterns were last *redefined upstream* for
//  1.21.13x and carried forward because nothing overrode them before 1.26;
//  the offsets reflect the 1.26 overrides. They are believed correct for
//  1.26.40 but have not been verified against a 1.26.40 binary by this
//  project. If Glacier logs unresolved signatures on 1.26.40, that belief was
//  wrong and the fix is here — see docs/reverse-engineering.md.
//
//  Multi-build support, when it is needed, goes through
//  SignatureManager::checkAboveOrEqual: seed the older value unconditionally,
//  then override it behind a version gate.
// ─────────────────────────────────────────────────────────────────────────────

#include "SignatureManager.h"

namespace glacier::memory {

void SignatureManager::seedBedrock() {
    if (m_seeded) return;
    m_seeded = true;

    // Phase 1 scope: only what the hook core, the SDK's ClientInstance capture,
    // and Fullbright actually need. The remaining entries (attack/ping/time
    // instrumentation, inventory + armor containers, the world→screen matrix
    // chain) are imported alongside the modules that consume them, so an
    // unresolved-signature warning always corresponds to a feature that exists.

    // ── Signatures (IDA-style byte patterns) ──

    // ClientInstance::update — hooked to capture the live ClientInstance
    // pointer each tick. Preferred over chasing a global, which moves between
    // builds. Attach-blocking: without it the SDK can never reach the player.
    addSignature("ClientInstance::update",
        "48 89 5c 24 ? 48 89 74 24 ? 55 57 41 56 48 8d 6c 24 ? 48 81 ec ? ? ? ? 48 8b f1 e8 ? ? ? ? 48 8b d8");

    // ClientInstance::getLocalPlayerIndex — not a function to call, but a
    // virtual *call site*. The 4-byte displacement at sig+9 is getLocalPlayer's
    // byte offset into the vtable; /8 gives the index (decoded in GameSDK).
    // Attach-blocking for the same reason as above.
    addSignature("ClientInstance::getLocalPlayerIndex",
        "49 8B 00 49 8B C8 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? 48 8B C8");

    // Options::getGamma — hooked by Fullbright, which returns an override value
    // instead of the player's real brightness while enabled. Not attach-
    // blocking: if this is missing, Fullbright no-ops and everything else runs.
    addSignature("Options::getGamma",
        "48 83 EC 28 48 8B 01 48 8D 54 24 30 41 B8 36 00 00 00");

    // ItemStack::getMaxDamage(ItemStack*) -> int. Called (not hooked) to fill in
    // durability bars. Optional: without it, bars simply don't render.
    addSignature("ItemStack::getMaxDamage",
        "48 83 EC ? 48 8B 51 ? 33 C0 48 85 D2 74 ? 48 39 02 0F 95 C1");

    // ── Instrumentation hooks (Phase 5 HUDs) ──
    // All optional. Each drives exactly one HUD, which shows "--" when the
    // signature is missing rather than a wrong number.

    // GameMode::attack(GameMode*, Actor* target, bool) — hooked to observe melee
    // hits for the Reach display. Read-only: the detour records and calls
    // through, it never alters the attack. Shape is >= 1.21.50.
    addSignature("GameMode::attack",
        "48 89 ? ? ? 48 89 ? ? ? 48 89 ? ? ? 55 41 ? 41 ? 41 ? 41 ? 48 8D ? ? ? ? ? ? 48 81 EC ? ? ? ? 48 8B ? ? ? ? ? 48 33 ? 48 89 ? ? ? ? ? 45 0F ? ? 4C 8B ? 48 8B ? 45 33 ? 44 89");

    // RakPeer::GetAveragePing — hooked to cache the live RTT whenever the game
    // asks for it. We never call it ourselves: calling into RakNet off the
    // network thread is not safe, so the HUD shows the last value the game
    // itself requested.
    addSignature("RakPeer::GetAveragePing",
        "48 8B C4 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? 0F 10 4A ? 4C 8B 1A 4C 3B 1D ? ? ? ? 0F 10 42 ? 48 89 58 ? 48 8B D9 0F 10 52 ? 0F 10 5A ? 0F 10 62 ? 0F 10 6A ? 0F 29 70 ? 0F 10 72 ? 0F 29 78 ? 0F B7 82 ? ? ? ? 0F 10 BA ? ? ? ? 66 89 45 ? 0F B7 82 ? ? ? ? 66 89 45 ? 0F 11 4C 24 ? 74 ? 44 8B 49");

    // Time-of-day helper (the `time % 24000` computation) — hooked read-only so
    // the Day Counter can derive absolute world time.
    addSignature("TimeChanger", "44 8B C2 B8 ? ? ? ? F7 EA");

    // ── Offsets ──

    // Actor::position — Vec3 of the entity's feet. Carried from 1.21.5x and not
    // overridden through 1.26.
    addOffset("Actor::position", 0x44);

    // GameMode -> owning Player. Stable since 1.20.30.
    addOffset("Gamemode::player", 0x8);

    // ── Player containers (armor + inventory HUDs) ──
    // These move between builds more often than anything else here; treat them
    // as the first suspects when a HUD shows nonsense rather than nothing.
    addOffset("Actor::armorContainer",         0x1670);
    addOffset("Player::supplies",              0x9B8);   // -> PlayerInventory*
    addOffset("PlayerInventory::container",    0x70);    // hotbar + main
    addOffset("PlayerInventory::selectedSlot", 0x10);

    // ── ItemStack memory layout ──
    // A Bedrock container is a contiguous std::vector<ItemStack>: the container
    // base holds [begin, end) pointers, and slots are indexed by stride.
    // Registered as named offsets rather than inlined in GameSDK so that every
    // build-specific number in the project stays inside this one file — see the
    // containment rule in docs/acknowledgements.md.
    addOffset("Container::begin",        0x00);
    addOffset("Container::end",          0x08);
    addOffset("ItemStack::stride",       0x88);
    addOffset("ItemStack::item",         0x08);   // Item*; null == air
    addOffset("ItemStack::count",        0x20);   // uint8
    addOffset("ItemStack::auxValue",     0x22);   // int16, used as damage
}

} // namespace glacier::memory
