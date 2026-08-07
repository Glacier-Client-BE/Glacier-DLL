# Signatures & Offsets

Every build-specific address in Glacier is resolved at runtime from a byte
pattern registered in [`src/memory/Signatures.cpp`](../src/memory/Signatures.cpp).
This file tracks what each one is for, where it came from, and whether it has
been confirmed against a live game.

**Provenance note:** all entries below are *imported* — derived from the public
reverse-engineering work in Flarial (AGPLv3) and Latite (GPLv3), not
independently derived by this project. That is why Glacier is AGPLv3; see
[acknowledgements.md](acknowledgements.md).

**Target build:** Minecraft: Bedrock Edition 1.26.x

## Status legend

| Status | Meaning |
|---|---|
| ✅ confirmed | Observed resolving against a running game, feature verified working |
| ⚠️ inherited | Imported and believed correct for the target build, **never verified in-game by this project** |
| ❌ unresolved | Known not to resolve; needs re-deriving |
| ⬜ not imported | Needed by a future phase, deliberately not seeded yet |

Nothing is marked ✅ until someone has injected a build and watched the feature
work. Compiling is not confirmation.

## Phase 1 — seeded

| Name | Kind | Purpose | Attach-blocking | Status |
|---|---|---|---|---|
| `ClientInstance::update` | signature | Hooked to capture the live `ClientInstance` each tick | **yes** | ⚠️ inherited |
| `ClientInstance::getLocalPlayerIndex` | signature | Call site whose displacement encodes `getLocalPlayer`'s vtable index | **yes** | ⚠️ inherited |
| `Options::getGamma` | signature | Hooked by Fullbright to return an override brightness | no | ⚠️ inherited |
| `Actor::position` | offset `0x44` | `Vec3` of an entity's feet | no | ⚠️ inherited |

Attach-blocking entries cause `GameSDK::resolve()` to refuse the attach rather
than continue into undefined behaviour. Non-blocking ones degrade the single
feature that depends on them and log why.

## Not yet imported

Deliberately left out of Phase 1. Each arrives with the module that consumes it,
so that an unresolved-signature warning always corresponds to a feature that
actually exists.

| Name | Kind | Needed for | Phase |
|---|---|---|---|
| `GameMode::attack` | signature | Hit Ping, Reach Display | 3 |
| `RakPeer::GetAveragePing` | signature | Ping display | 3 |
| `TimeChanger` | signature | Day Counter | 3 |
| `Level::getRuntimeActorList` | signature | Hitboxes (entity enumeration) | 3 |
| `BobHurt` | signature | Java View Bobbing | 3 |
| `MinimalViewBobbing` | signature | Minimal View Bobbing (NOP patch site) | 3 |
| `ForceCoordsOption` | signature | Force Coords | 3 |
| `AppPlatform::readAssetFile` | signature | Material Bin Loader | 3 |
| `LocalPlayer::applyTurnDelta` | signature | Snap Look | 3 |
| `ItemStack::getMaxDamage` | signature | Durability bars in HUDs | 3 |
| `Inventory::addItem` | signature | Container manipulation reference | 3 |
| `Player::supplies`, `PlayerInventory::*`, `Actor::armorContainer` | offsets | Armor / Inventory HUDs | 3 |
| `GameRenderer::viewMatrix`, `GameRenderer::projMatrix`, `LevelRenderer*`, `GuiData::screenSize` | offsets | World→screen projection for Hitboxes | 3 |
| `ClientInstance::minecraftGame`, `guiData`, `levelRenderer`, `camera`, `packetSender` | offsets | Various | 3 |

**Phase 3's real progress metric is this table**, not module count — the modules
are mechanical once their signatures resolve.

## When these stop working

A game update moves code, and patterns stop matching. The symptom is explicit:
`scanAll()` logs every unresolved name, and the client refuses to attach if a
blocking one is missing. See [reverse-engineering.md](reverse-engineering.md)
for how to re-derive them.
