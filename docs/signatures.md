# Signatures & Offsets

Every build-specific address in Glacier is resolved at runtime from a byte
pattern registered in [`src/memory/Signatures.cpp`](../src/memory/Signatures.cpp).
This file tracks what each one is for, where it came from, and whether it has
been confirmed against a live game.

**Provenance note:** all entries below are *imported* — derived from the public
reverse-engineering work in Flarial (AGPLv3) and Latite (GPLv3), not
independently derived by this project. That is why Glacier is AGPLv3; see
[acknowledgements.md](acknowledgements.md).

**Target build:** Minecraft: Bedrock Edition **1.26.40**

Declared in `SignatureManager::kTarget{Major,Minor,Patch}`. Glacier reads the
attached game's real version from its executable version resource at startup
and logs it, warning when it differs from the target:

```
[Glacier][info] game build 1.26.40.0 (Glacier targets 1.26.40)
```

or, on a mismatch:

```
[Glacier][warn] game build 1.26.31 differs from the targeted 1.26.40 —
                signatures may not resolve, and offsets may read wrong data.
```

The version check uses no signature, so it still works when every pattern in
the table has gone stale — which is exactly when it matters most.

**Important caveat about 1.26.40 specifically.** These patterns were last
*redefined upstream* for 1.21.13x and carried forward because nothing overrode
them before 1.26; the offsets reflect the 1.26 overrides. They are believed
correct for 1.26.40 but **have not been verified against a 1.26.40 binary by
this project**. Nobody has run this against that build. If the log reports
unresolved signatures, that belief was wrong, and the fix is one file.

## Status legend

| Status | Meaning |
|---|---|
| ✅ confirmed | Observed resolving against a running game, feature verified working |
| ⚠️ inherited | Imported and believed correct for the target build, **never verified in-game by this project** |
| ❌ unresolved | Known not to resolve; needs re-deriving |
| ⬜ unused | Seeded and available, but nothing reads it yet — so a wrong value here breaks nothing today |
| ⬜ not imported | Needed by a future phase, deliberately not seeded yet |

Nothing is marked ✅ until someone has injected a build and watched the feature
work. Compiling is not confirmation.

## Seeded

This table mirrors `seedBedrock()` in
[`src/memory/Signatures.cpp`](../src/memory/Signatures.cpp) entry for entry. If
you add or remove a mapping in `tools/sync_signatures.py`, update this table in
the same commit — a table that disagrees with the generated file is worse than
no table, because it gets trusted.

### Signatures

| Name | Purpose | Attach-blocking | Status |
|---|---|---|---|
| `Platform_GameCore` | The global that roots the whole object graph. Not called — the RIP-relative operand at +3 is decoded to find the global itself | **yes** | ⚠️ inherited |
| `Options::getGamma` | Hooked by Fullbright to return an override brightness | no | ⚠️ inherited |
| `RakPeer::GetAveragePing` | Hooked read-only to cache RTT for Ping display | no | ⚠️ inherited |
| `Dimension::getTimeOfDay` | Hooked read-only to cache world time for Day Counter | no | ⚠️ inherited |
| `ClientInstance::grabCursor` | Called on menu close to hand the mouse back to gameplay | no | ⚠️ inherited |
| `ClientInstance::releaseCursor` | Called on menu open — this is what actually pauses look/move | no | ⚠️ inherited |
| `Actor::attack` | Observed read-only for Reach Display. `this` is the attacker, so no `GameMode` → player hop | no | ⚠️ inherited |

### Offsets

| Name | Value | Purpose | Attach-blocking | Status |
|---|---|---|---|---|
| `WinMain::platformGameCore` | `0x08` | Deref of the `Platform_GameCore` global → `WinMain`; +0x08 → `Platform_GameCore*` | no¹ | ⚠️ inherited |
| `Platform_GameCore::minecraftGame` | `0x18` | → `MinecraftGame*` | no¹ | ⚠️ inherited |
| `MinecraftGame::clientInstances` | `0x938` | `map<uint8, shared_ptr<ClientInstance>>`; entry 0 is the local one | no¹ | ⚠️ inherited |
| `ClientInstance::getLocalPlayerVIndex` | `31` | vtable **index** (not a byte offset) of `getLocalPlayer` | **yes** | ⚠️ inherited |
| `ClientInstance::minecraftGame` | `0x1A0` | Seeded, not yet consumed | no | ⬜ unused |
| `ClientInstance::levelRenderer` | `0x1B8` | Seeded, not yet consumed | no | ⬜ unused |
| `ClientInstance::packetSender` | `0x1C8` | Seeded, not yet consumed | no | ⬜ unused |
| `ClientInstance::guiData` | `0x648` | Seeded, not yet consumed — Phase 7 needs it for GUI scale | no | ⬜ unused |
| `ClientInstance::options` | `0xD78` | Seeded, not yet consumed | no | ⬜ unused |
| `Actor::entityContext` | `0x08` | Embedded `EntityContext` — the route into the entt registry, used for armor | no | ⚠️ inherited |
| `Actor::stateVector` | `0x218` | → `StateVectorComponent*`; position lives in an ECS component | no | ⚠️ inherited |
| `StateVectorComponent::pos` | `0x00` | `Vec3` position (`IEntityComponent` is an empty base) | no | ⚠️ inherited |
| `Player::supplies` | `0x5B8` | → `PlayerInventory*` | no | ⚠️ inherited |
| `PlayerInventory::selectedSlot` | `0x10` | Which hotbar slot is held | no | ⚠️ inherited |
| `PlayerInventory::inventory` | `0xB8` | → the backing `Inventory` | no | ⚠️ inherited |
| `Inventory::getItemVIndex` | `7` | vtable **index** of `Inventory::getItem(int)` — the game bounds-checks for us | no | ⚠️ inherited |
| `ItemStack::item` | `0x08` | `Item**`; null means the slot is air | no | ⚠️ inherited |
| `ItemStack::auxValue` | `0x20` | `int16`, read as the damage value | no | ⚠️ inherited |
| `ItemStack::count` | `0x22` | `uint8` stack size | no | ⚠️ inherited |
| `ItemStack::size` | `0x98` | `sizeof(ItemStack)`. Seeded, not yet consumed — every container read goes through the virtual `getItem`, so no stride arithmetic happens | no | ⬜ unused |

¹ Not checked individually at attach, but the chain they form is what
`clientInstance()` walks. A wrong value here degrades to `nullptr` — every
SDK-backed HUD shows `--` — rather than crashing.

### Which modules need which

| Module | Needs | If it breaks |
|---|---|---|
| Watermark, FPS Counter, Keystrokes, CPS Counter, Clock, Module List, Null Movement | **nothing** | Can only break if the overlay itself is broken |
| Fullbright | `Options::getGamma` | Toggling does nothing; logs a warning on enable |
| Coordinates | the object-graph chain, `Actor::stateVector`, `StateVectorComponent::pos` | Shows `XYZ --`, or numbers that don't track movement |
| Armor HUD | `Actor::entityContext` (+ the entt pin), `Inventory::getItemVIndex`, the `ItemStack` offsets | Empty slots while armoured, or nonsense counts |
| Ping | `RakPeer::GetAveragePing` | Shows `--` |
| Day Counter | `Dimension::getTimeOfDay` | Shows `Day --` |
| Reach Display | `Actor::attack`, `Actor::stateVector` | Shows `Reach --` after a hit |
| Menu (pause) | `ClientInstance::grabCursor` / `releaseCursor` | Menu still opens, but you can move and look behind it; a warning is logged at attach |

Coordinates is the cheapest end-to-end check of the SDK: if it tracks your
movement, the whole signature → global → `ClientInstance` → `LocalPlayer` chain
works. Check it before debugging anything else.

Attach-blocking entries cause `GameSDK::resolve()` to refuse the attach rather
than continue into undefined behaviour. Non-blocking ones degrade the single
feature that depends on them and log why.

### Known gap: durability is never populated

`ItemStack::maxDurability` is declared in `sdk::GameSDK::ItemStack` and read by
Armor HUD, but `readStack` never assigns it — no signature for the max-damage
lookup has been imported. It is therefore always `0`, which means
`durabilityFraction()` always returns `1.0` and **Armor HUD's durability bars
never draw**, silently. Closing this needs `ItemStackBase::getDamageValue` and a
max-damage source; both exist upstream in Latite's `Addresses.h`.

## Not yet imported

Each arrives with the feature that consumes it, so an unresolved-signature
warning always corresponds to something that actually exists.

| Name | Kind | Needed for | Phase |
|---|---|---|---|
| `ItemRenderer::renderGuiItemNew` | signature | Drawing real item icons | 7 |
| `BaseActorRenderContext::BaseActorRenderContext` | signature | Constructing the render context `renderGuiItemNew` needs | 7 |
| `ItemStackBase::getDamageValue` | signature | Real durability values (see the gap above) | 7 |
| `ScreenContext` / `GuiData::screenSize` | offsets | GUI-space coordinates for item draws | 7 |
| `Level::getRuntimeActorList` | signature | Entity enumeration (Target HUD, Player List) | 8 |
| `LocalPlayer::applyTurnDelta` | signature | Snap Look | 8 |
| `GameRenderer::viewMatrix`, `GameRenderer::projMatrix` | offsets | World→screen projection | 8 |

**Phase 7's real progress metric is this table**, not module count — the modules
are mechanical once their signatures resolve.

## Keeping this in sync

`src/memory/Signatures.cpp` is **generated**. Do not hand-edit it.

```bash
python tools/sync_signatures.py
```

It regenerates the table from [Latite](https://github.com/LatiteClient/Latite),
which actively supports this build. A scheduled workflow
(`.github/workflows/sync-signatures.yml`) runs it daily and opens a PR when
anything changes — deliberately a PR, not a push, because signature changes
steer memory reads inside another process and deserve a human reading the diff.

The mapping in the script is explicit on both sides, so an upstream rename
produces a loud failure and writes nothing, rather than silently dropping the
entry whose feature then quietly stops working.

**A green sync PR is not verification.** It proves the table compiles, not that
the values are right for your game. Entries stay ⚠️ inherited until someone
injects and confirms.

## Armor, and the entt pin

Armor is the one piece of data with no signature or offset of its own. It lives
in an `ActorEquipmentComponent` inside Bedrock's entt registry, reached via
`Actor::entityContext`.

That makes `third_party/entt` load-bearing. It is a submodule pinned to commit
`fe8d7d78` — the exact commit Latite uses — because the lookup requires our
entt to agree bit-for-bit with the game's: entity-id bit layout, sparse-set
page sizes, storage type. A different entt version still compiles and still
appears to work, right up until it indexes the wrong page and hands back a
component belonging to another entity.

If armor ever shows plausible-but-wrong values (rather than nothing), suspect
this pin before suspecting any offset.

### Alternative route, not taken

Molang queries (`query.armor_texture_slot`, `query.armor_damage_slot`) expose
armor state too, and would sidestep entt entirely. They were not used here
because evaluating Molang needs the interpreter plus a live render context, and
they surface *rendering* state — which texture, which damage tier — rather than
the `ItemStack` the HUD wants for counts and durability. Worth revisiting if the
entt pin becomes a maintenance problem, or for drawing real armor icons, which
the component route cannot do.

## When these stop working

A game update moves code, and patterns stop matching. The symptom is explicit:
`scanAll()` logs every unresolved name, and the client refuses to attach if a
blocking one is missing. See [reverse-engineering.md](reverse-engineering.md)
for how to re-derive them.
