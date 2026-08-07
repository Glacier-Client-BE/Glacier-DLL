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
| ⬜ not imported | Needed by a future phase, deliberately not seeded yet |

Nothing is marked ✅ until someone has injected a build and watched the feature
work. Compiling is not confirmation.

## Seeded

| Name | Kind | Purpose | Attach-blocking | Status |
|---|---|---|---|---|
| `ClientInstance::update` | signature | Hooked to capture the live `ClientInstance` each tick | **yes** | ⚠️ inherited |
| `ClientInstance::getLocalPlayerIndex` | signature | Call site whose displacement encodes `getLocalPlayer`'s vtable index | **yes** | ⚠️ inherited |
| `Options::getGamma` | signature | Hooked by Fullbright to return an override brightness | no | ⚠️ inherited |
| `ItemStack::getMaxDamage` | signature | Called for durability bars | no | ⚠️ inherited |
| `GameMode::attack` | signature | Observed (read-only) for Reach Display | no | ⚠️ inherited |
| `RakPeer::GetAveragePing` | signature | Caches RTT for Ping display | no | ⚠️ inherited |
| `TimeChanger` | signature | Caches world time for Day Counter | no | ⚠️ inherited |
| `Gamemode::player` | offset `0x8` | GameMode -> owning Player, for Reach | no | ⚠️ inherited |
| `Actor::position` | offset `0x44` | `Vec3` of an entity's feet — Coordinates | no | ⚠️ inherited |
| `Actor::armorContainer` | offset `0x1670` | Armor HUD | no | ⚠️ inherited |
| `Player::supplies` | offset `0x9B8` | `PlayerInventory*` — held item | no | ⚠️ inherited |
| `PlayerInventory::container` | offset `0x70` | Hotbar + main container | no | ⚠️ inherited |
| `PlayerInventory::selectedSlot` | offset `0x10` | Which hotbar slot is held | no | ⚠️ inherited |
| `Container::begin` / `Container::end` | offsets `0x00` / `0x08` | `std::vector<ItemStack>` bounds | no | ⚠️ inherited |
| `ItemStack::stride` | offset `0x88` | Slot stride within a container | no | ⚠️ inherited |
| `ItemStack::item` / `count` / `auxValue` | offsets `0x08` / `0x20` / `0x22` | Slot contents | no | ⚠️ inherited |

### Which modules need which

| Module | Needs | If it breaks |
|---|---|---|
| Watermark, FPS Counter, Keystrokes, CPS Counter, Clock, Module List, Null Movement | **nothing** | Can only break if the overlay itself is broken |
| Fullbright | `Options::getGamma` | Toggling does nothing; logs a warning on enable |
| Coordinates | `ClientInstance::*`, `Actor::position` | Shows `XYZ --`, or numbers that don't track movement |
| Armor HUD | the container + `ItemStack` offsets | Empty slots while armoured, or nonsense counts |
| Ping | `RakPeer::GetAveragePing` | Shows `--` |
| Day Counter | `TimeChanger` | Shows `Day --` |
| Reach Display | `GameMode::attack`, `Gamemode::player` | Shows `Reach --` after a hit |

Coordinates is the cheapest end-to-end check of the SDK: if it tracks your
movement, the whole signature → hook → `ClientInstance` → `LocalPlayer` chain
works. Check it before debugging anything else.

Attach-blocking entries cause `GameSDK::resolve()` to refuse the attach rather
than continue into undefined behaviour. Non-blocking ones degrade the single
feature that depends on them and log why.

## Not yet imported

Deliberately left out of Phase 1. Each arrives with the module that consumes it,
so that an unresolved-signature warning always corresponds to a feature that
actually exists.

| Name | Kind | Needed for | Phase |
|---|---|---|---|
| `Level::getRuntimeActorList` | signature | Hitboxes (entity enumeration) | 5 |
| `BobHurt` | signature | Java View Bobbing | 5 |
| `MinimalViewBobbing` | signature | Minimal View Bobbing (NOP patch site) | 5 |
| `ForceCoordsOption` | signature | Force Coords | 5 |
| `AppPlatform::readAssetFile` | signature | Material Bin Loader | 5 |
| `LocalPlayer::applyTurnDelta` | signature | Snap Look | 5 |
| `GameRenderer::viewMatrix`, `GameRenderer::projMatrix`, `LevelRenderer*`, `GuiData::screenSize` | offsets | World→screen projection for Hitboxes | 5 |
| `ClientInstance::minecraftGame`, `guiData`, `levelRenderer`, `camera`, `packetSender` | offsets | Various | 5 |

**Phase 5's real progress metric is this table**, not module count — the modules
are mechanical once their signatures resolve.

## When these stop working

A game update moves code, and patterns stop matching. The symptom is explicit:
`scanAll()` logs every unresolved name, and the client refuses to attach if a
blocking one is missing. See [reverse-engineering.md](reverse-engineering.md)
for how to re-derive them.
