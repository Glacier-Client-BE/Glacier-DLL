// ─────────────────────────────────────────────────────────────────────────────
//  THIRD-PARTY-DERIVED DATA — READ docs/acknowledgements.md BEFORE EDITING
//
//  GENERATED FILE. Do not edit by hand; your changes will be overwritten.
//  Regenerate with:  python tools/sync_signatures.py
//
//  Source: Latite (LatiteClient/Latite) @ fe0b09741c07 — GPLv3
//  Also informed by Flarial (dll-oss) — AGPLv3
//
//  These AOB patterns and struct offsets were NOT independently derived. This
//  single file is the reason Glacier is licensed AGPLv3 rather than
//  permissively; AGPLv3 absorbs both upstreams cleanly (GPLv3 material may be
//  incorporated under GPLv3 §13).
//
//  Two rules follow, and they are why this file is separate from
//  SignatureManager.cpp:
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
//  Declared in SignatureManager::kTarget{Major,Minor,Patch}. The attached
//  game's real version is read at startup and compared, so a mismatch is
//  logged rather than discovered through weird behaviour.
//
//  Values come from a project that actively supports this build, which is
//  meaningfully stronger evidence than a carried-forward table — but they have
//  still not been verified against a running client by this project. See
//  docs/signatures.md.
// ─────────────────────────────────────────────────────────────────────────────

#include "SignatureManager.h"

namespace glacier::memory {

void SignatureManager::seedBedrock() {
    if (m_seeded) return;
    m_seeded = true;

    // ── Signatures ──

    // The root of the object graph. A `mov [rip+disp], r15` store into a
    // global; the RIP-relative operand at +3 addresses the global itself,
    // so this resolves to the global rather than to the instruction.
    // Data, not code — it is read, never called.
    addSignature("Platform_GameCore",
        "4C 89 3D ? ? ? ? 4D 85 FF",
        /*deref*/ 3, TargetKind::Data);

    // Hooked by Fullbright to return an override brightness. The trailing
    // immediate is the option index and it moves between builds, which is
    // what makes this pattern unusually version-sensitive. If Fullbright
    // breaks first after an update, look here.
    addSignature("Options::getGamma",
        "48 83 EC 38 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 48 8B 01 48 8B 40 08 48 8D 54 24 ? 41 B8 35 00 00 00");

    // Hooked read-only to cache the RTT the game itself queries. We never
    // call it: reaching into RakNet off the network thread isn't safe.
    addSignature("RakPeer::GetAveragePing",
        "48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 31 E0 48 89 84 24 ? ? ? ? 4C 8B 02 4C 3B 05 ? ? ? ? 0F 85 ? ? ? ? 0F B7 42 ? 44 0F B7 82 ? ? ? ? 44 0F B7 8A ? ? ? ? 66 89 44 24 ? 0F 10 42 ? 0F 10 4A ? 0F 10 52 ? 0F 10 5A ? 0F 11 44 24 ? 0F 11 4C 24 ? 0F 11 54 24 ? 0F 11 5C 24 ? 0F 10 42 ? 0F 11 44 24 ? 0F 10 42 ? 0F 11 44 24 ? 0F 10 42 ? 0F 11 84 24 ? ? ? ? 0F 10 82 ? ? ? ? 0F 11 84 24 ? ? ? ? 66 44 89 8C 24 ? ? ? ? 66 44 89 84 24 ? ? ? ? 48 8D 54 24 ? 45 31 C0 45 31 C9 E8 ? ? ? ? BA");

    // Hooked read-only for the Day Counter.
    addSignature("Dimension::getTimeOfDay",
        "48 63 C2 48 69 C8 ? ? ? ? 48 89 CA 48 C1 EA ? 48 C1 F9");

    // Called directly (not hooked) to hand the cursor back to the game when
    // the menu closes. Driving the game's own grab state is what actually
    // pauses look/move input — Bedrock reads RawInput, so swallowing window
    // messages does nothing.
    addSignature("ClientInstance::grabCursor",
        "56 48 83 EC ? 48 89 CE 48 8B 01 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 74 ? 48 8B 8E ? ? ? ? 48 8B 01 48 8B 80 ? ? ? ? 48 8B 15 ? ? ? ? 48 83 C4 ? 5E 48 FF E2 90 48 83 C4 ? 5E C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 56 48 83 EC");
    // The other half of the pair, called when the menu opens.
    addSignature("ClientInstance::releaseCursor",
        "56 48 83 EC ? 48 89 CE 48 8B 01 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 74 ? 48 8B 8E ? ? ? ? 48 8B 01 48 8B 80 ? ? ? ? 48 8B 15 ? ? ? ? 48 83 C4 ? 5E 48 FF E2 90 48 83 C4 ? 5E C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 56 53");

    // Observed read-only for the Reach display. `this` is the attacker, so
    // no GameMode->player indirection is needed.
    addSignature("Actor::attack",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 4D 89 CF 4D 89 C6 48 89 D6 48 89 CF 41 8B 80");

    // Hooked to get a foothold inside the game's UI render pass. The second
    // argument is a MinecraftUIRenderContext, which carries the live
    // ScreenContext that item drawing requires. Matched at a call site, so
    // the displacement at +1 has to be followed to reach the function.
    addSignature("ScreenView::setupAndRender",
        "E8 ? ? ? ? 48 8B 4B ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 89 FA FF 15 ? ? ? ? 48 8D 4D",
        /*deref*/ 1);
    // Constructor, called on a zeroed stack buffer to build the context
    // renderGuiItemNew wants. Not paired with a destructor call: upstream
    // leaves the object to go out of scope the same way, and the type owns
    // nothing we allocated.
    addSignature("BaseActorRenderContext::BaseActorRenderContext",
        "55 41 56 56 57 53 48 83 EC ? 48 8D 6C 24 ? 48 C7 45 ? ? ? ? ? 4C 89 C6 48 89 D7 49 89 CE 48 8D 05 ? ? ? ? 48 89 01 0F 57 C0");
    // The actual icon draw. Also matched at a call site (deref 1). Argument
    // order is NOT the declaration order — see sdk/ItemRendering.cpp.
    addSignature("ItemRenderer::renderGuiItemNew",
        "E8 ? ? ? ? 48 8D 55 ? 4C 8D 85 ? ? ? ? 48 89 F1 E8 ? ? ? ? 80 BF",
        /*deref*/ 1);
    // Real damage value for durability bars. The raw auxValue field is not
    // the damage for every item, which is why this is a call and not an
    // offset read.
    addSignature("ItemStackBase::getDamageValue",
        "56 57 48 83 EC ? 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 48 8B 41 ? 48 85 C0 74 ? 48 83 38");

    // ── Offsets ──
    // *(Platform_GameCore sig deref) -> winMain; +0x08 -> Platform_GameCore*
    addOffset("WinMain::platformGameCore", 0x08);
    // Platform_GameCore::getMinecraftGame
    addOffset("Platform_GameCore::minecraftGame", 0x18);
    // map<uint8, shared_ptr<ClientInstance>>; entry 0 is the local one
    addOffset("MinecraftGame::clientInstances", 0x938);
    // bool, from MinecraftGame::isCursorGrabbed. Read every frame while
    // the menu is open: the game re-grabs the cursor on its own, so a
    // single releaseCursor() call does not hold.
    addOffset("MinecraftGame::cursorGrabbed", 0x1D8);

    addOffset("ClientInstance::minecraftGame", 0x1A0);
    addOffset("ClientInstance::levelRenderer", 0x1B8);
    addOffset("ClientInstance::packetSender", 0x1C8);
    // ClientInstance::getGuiData
    addOffset("ClientInstance::guiData", 0x648);
    // ClientInstance::getOptions
    addOffset("ClientInstance::options", 0xD78);
    // vtable *index*, not a byte offset (ClientInstance::getLocalPlayer)
    addOffset("ClientInstance::getLocalPlayerVIndex", 31);

    // Embedded EntityContext — the route into the entt registry for
    // component data such as armor. See src/sdk/EntityComponents.h.
    addOffset("Actor::entityContext", 0x08);
    // Position lives in an ECS component; Actor caches a pointer to it.
    addOffset("Actor::stateVector", 0x218);
    // IEntityComponent is an empty base, so pos sits at offset 0
    addOffset("StateVectorComponent::pos", 0x00);

    addOffset("Player::supplies", 0x5B8);
    addOffset("PlayerInventory::selectedSlot", 0x10);
    addOffset("PlayerInventory::inventory", 0xB8);
    // Inventory::getItem(int) vtable index — the game bounds-checks for us
    addOffset("Inventory::getItemVIndex", 7);

    // Plain members in Latite's MinecraftUIRenderContext.h, not CLASS_FIELD:
    //   class ClientInstance* cinst;  ScreenContext* screenContext;
    // They are declared FIRST in the header but do NOT start at 0. The
    // class declares virtual functions further down, so MSVC puts the
    // vtable pointer at 0x00 and the members follow it — declaration
    // order in the source is not layout order when a vptr exists.
    // Reading these at 0x00/0x08 handed the item renderer a vtable
    // pointer as its ClientInstance, which then produced a garbage
    // MinecraftGame and crashed the game inside the render-context
    // constructor. Flarial lists the same 0x8/0x10 independently.
    addOffset("MinecraftUIRenderContext::clientInstance", 0x08);
    addOffset("MinecraftUIRenderContext::screenContext", 0x10);
    // Screen pixels per GUI unit
    addOffset("GuiData::guiScale", 0x5C);
    // 1/guiScale. The game keeps both; we read this one because
    // every conversion we do is pixels -> GUI units.
    addOffset("GuiData::guiScaleFrac", 0x60);
    addOffset("GuiData::screenSize", 0x40);
    addOffset("BaseActorRenderContext::itemRenderer", 0x58);
    // Latite declares `char pad[0x500]` with a "TODO: check actual
    // size" — its own admission the number was never measured. Flarial
    // and Lyra independently pad the same type to 0x1000 (`char
    // filling[4096]`), specifically so the constructor's writes cannot
    // run past what the caller reserved. Trust the larger, better-
    // attested figure: over-reserving costs stack space, under-
    // reserving lets the game corrupt memory past our buffer.
    addOffset("BaseActorRenderContext::size", 0x1000);
    // Item::getMaxDamage() vtable index — 0 for anything not damageable
    addOffset("Item::getMaxDamageVIndex", 36);

    // static_assert(sizeof(ItemStack) == 0x98) in Latite's ItemStack.h
    addOffset("ItemStack::size", 0x98);
    // Item**; null == air
    addOffset("ItemStack::item", 0x08);
    // int16
    addOffset("ItemStack::auxValue", 0x20);
    // uint8
    addOffset("ItemStack::count", 0x22);
}

} // namespace glacier::memory
