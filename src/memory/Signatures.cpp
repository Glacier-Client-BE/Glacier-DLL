// ─────────────────────────────────────────────────────────────────────────────
//  THIRD-PARTY-DERIVED DATA — READ docs/acknowledgements.md BEFORE EDITING
//
//  The AOB byte patterns and struct offsets in this file are derived from the
//  open reverse-engineering work published in other Bedrock clients:
//
//      Latite   https://github.com/LatiteClient/Latite   — GPLv3
//      Flarial  https://github.com/flarialmc/dll-oss     — AGPLv3
//
//  They were NOT independently derived. This single file is the reason Glacier
//  is licensed AGPLv3 rather than permissively; AGPLv3 absorbs both upstreams
//  cleanly (GPLv3 material may be incorporated under GPLv3 §13).
//
//  Two rules follow, and they are the whole point of this file existing
//  separately from SignatureManager.cpp:
//
//    1. Keep imported data HERE. Never inline a pattern or a struct offset
//       anywhere else in the tree — everything build-specific is looked up
//       through SignatureManager by name.
//
//    2. Everything else in Glacier (the scanner, the registry, the hooks, the
//       module system, the UI) is Glacier's own code and carries no such
//       constraint.
//
//  ── TARGET BUILD: Minecraft: Bedrock Edition 1.26.40 ──
//
//  This table was rewritten from Latite's current (1.26.4x-era) data. The
//  previous table carried patterns last redefined for 1.21.13x, which had
//  drifted badly — Options::getGamma had a different prologue AND a different
//  option index, Player::supplies had moved 0x400 bytes, entity position had
//  migrated into an ECS component, and the ItemStack count/aux offsets were
//  transposed. Those are exactly the failures the version warning exists to
//  explain.
//
//  Still unverified by this project against a running 1.26.40 client: these
//  values are taken from an actively-maintained project that supports the
//  build, which is much stronger evidence than before, but it is not the same
//  as having watched them work. See docs/signatures.md.
// ─────────────────────────────────────────────────────────────────────────────

#include "SignatureManager.h"

namespace glacier::memory {

void SignatureManager::seedBedrock() {
    if (m_seeded) return;
    m_seeded = true;

    // ── Signatures ──

    // The root of the object graph. This is a `mov [rip+disp], r15` store into
    // a global; the RIP-relative operand at +3 gives the global's address, and
    // the value there is the WinMain-ish object holding Platform_GameCore.
    // Resolved in GameSDK via offsetFromSig(sig, 3).
    addSignature("Platform_GameCore", "4C 89 3D ? ? ? ? 4D 85 FF");

    // Options::getGamma — hooked by Fullbright to return an override value.
    // Note the trailing `41 B8 35 00 00 00`: that immediate is the option index
    // and it moves between builds, which is what makes this pattern so
    // version-sensitive. If Fullbright breaks first after an update, look here.
    addSignature("Options::getGamma",
        "48 83 EC 38 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 48 8B 01 48 8B 40 08 48 8D 54 24 ? 41 B8 35 00 00 00");

    // RakPeer::GetAveragePing — hooked read-only to cache the RTT the game
    // itself queries. We never call it: reaching into RakNet off the network
    // thread is not safe.
    addSignature("RakPeer::GetAveragePing",
        "48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 31 E0 48 89 84 24 ? ? ? ? 4C 8B 02 4C 3B 05 ? ? ? ? 0F 85 ? ? ? ? 0F B7 42 ? 44 0F B7 82 ? ? ? ? 44 0F B7 8A ? ? ? ? 66 89 44 24 ? 0F 10 42 ? 0F 10 4A ? 0F 10 52 ? 0F 10 5A ? 0F 11 44 24 ? 0F 11 4C 24 ? 0F 11 54 24 ? 0F 11 5C 24 ? 0F 10 42 ? 0F 11 44 24 ? 0F 10 42 ? 0F 11 44 24 ? 0F 10 42 ? 0F 11 84 24 ? ? ? ? 0F 10 82 ? ? ? ? 0F 11 84 24 ? ? ? ? 66 44 89 8C 24 ? ? ? ? 66 44 89 84 24 ? ? ? ? 48 8D 54 24 ? 45 31 C0 45 31 C9 E8 ? ? ? ? BA");

    // Dimension::getTimeOfDay — hooked read-only for the Day Counter. Replaces
    // the old "TimeChanger" helper, which no longer exists in this shape.
    addSignature("Dimension::getTimeOfDay",
        "48 63 C2 48 69 C8 ? ? ? ? 48 89 CA 48 C1 EA ? 48 C1 F9");

    // Actor::attack(Actor* self, Actor* target, ...) — observed read-only for
    // the Reach display. Replaces GameMode::attack: `this` is now the attacker
    // directly, so no GameMode->player indirection is needed.
    addSignature("Actor::attack",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 4D 89 CF 4D 89 C6 48 89 D6 48 89 CF 41 8B 80");

    // ── Object-graph offsets ──
    // ClientInstance is reached by walking globals rather than by hooking an
    // update function: the chain is stable, needs no hook, and works before the
    // first tick.
    //
    //   *(Platform_GameCore sig deref)          -> winMain
    //   winMain + 0x08                          -> Platform_GameCore*
    //   Platform_GameCore + 0x18                -> MinecraftGame*
    //   MinecraftGame + 0x938                   -> map<uint8, shared_ptr<CI>>
    addOffset("WinMain::platformGameCore",       0x08);
    addOffset("Platform_GameCore::minecraftGame", 0x18);
    addOffset("MinecraftGame::clientInstances",  0x938);

    // ClientInstance
    addOffset("ClientInstance::minecraftGame",   0x1A0);
    addOffset("ClientInstance::levelRenderer",   0x1B8);
    addOffset("ClientInstance::packetSender",    0x1C8);
    addOffset("ClientInstance::guiData",         0x648);
    addOffset("ClientInstance::options",         0xD78);
    // Vtable *index*, not a byte offset.
    addOffset("ClientInstance::getLocalPlayerVIndex", 0x1F);

    // ── Actor ──
    // Position moved into an ECS component: Actor holds a cached pointer to its
    // StateVectorComponent, whose first field is the position. IEntityComponent
    // is an empty base, so pos sits at offset 0.
    addOffset("Actor::stateVector",              0x218);
    addOffset("StateVectorComponent::pos",       0x00);

    // ── Player containers ──
    // Held item is reachable; armor is not (see GameSDK::armor).
    addOffset("Player::supplies",                0x5B8);
    addOffset("PlayerInventory::selectedSlot",   0x10);
    addOffset("PlayerInventory::inventory",      0xB8);
    // Inventory::getItem(int) is virtual — far more robust than walking the
    // backing vector by hand, which is what the previous table did.
    addOffset("Inventory::getItemVIndex",        7);

    // ── ItemStack layout ──
    // The previous table had count and aux transposed and the stride 0x10 too
    // small. Corrected from ItemStackBase's declared field order.
    addOffset("ItemStack::size",                 0x98);
    addOffset("ItemStack::item",                 0x08);   // Item**; null == air
    addOffset("ItemStack::auxValue",             0x20);   // int16
    addOffset("ItemStack::count",                0x22);   // uint8
}

} // namespace glacier::memory
