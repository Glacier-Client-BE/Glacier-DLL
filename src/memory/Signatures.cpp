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
//  Target build: Minecraft: Bedrock Edition 1.26.x (patterns last redefined
//  upstream for 1.21.13x and not overridden before 1.26; offsets reflect the
//  1.26 overrides). See docs/signatures.md for per-entry provenance and
//  docs/reverse-engineering.md for the runbook when these stop resolving.
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

    // ── Offsets ──

    // Actor::position — Vec3 of the entity's feet. Carried from 1.21.5x and not
    // overridden through 1.26.
    addOffset("Actor::position", 0x44);

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
