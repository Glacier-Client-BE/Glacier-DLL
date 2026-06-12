#include "SignatureManager.h"

#include "Memory.h"
#include "../util/Logger.h"

namespace glacier::memory {

void SignatureManager::addSignature(std::string name, std::string idaPattern) {
    m_sigs[std::move(name)] = Entry{ std::move(idaPattern), 0 };
}

void SignatureManager::addOffset(std::string name, std::ptrdiff_t offset) {
    m_offsets[std::move(name)] = offset;
}

std::size_t SignatureManager::scanAll() {
    const ModuleRange game = moduleRange();
    std::size_t resolved = 0;
    for (auto& [name, entry] : m_sigs) {
        if (auto hit = findSignature(entry.pattern, game)) {
            entry.address = *hit;
            ++resolved;
        } else {
            entry.address = 0;
            LOG_WARN("signature not found: {}", name);
        }
    }
    LOG_INFO("resolved {}/{} signatures", resolved, m_sigs.size());
    return resolved;
}

std::uintptr_t SignatureManager::sig(std::string_view name) const {
    auto it = m_sigs.find(name);
    return it != m_sigs.end() ? it->second.address : 0;
}

std::ptrdiff_t SignatureManager::offset(std::string_view name) const {
    auto it = m_offsets.find(name);
    return it != m_offsets.end() ? it->second : 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Seed table — effective signatures & offsets for Minecraft: Bedrock 1.26.x.
//
//  Flarial loads signatures/offsets cumulatively (oldest version first, newer
//  versions override), so the "1.26" set = the accumulation up to init260. The
//  signatures below were last (re)defined for 1.21.13x and are NOT overridden
//  before 1.26, so they remain the live values for 1.26.x. The offsets reflect
//  init260's overrides (gameRenderer 0xD70, Block 0x60, …). Sourced from the
//  open-source RE work in Flarial (dll-oss) and Latite.
// ─────────────────────────────────────────────────────────────────────────────
void SignatureManager::seedBedrock() {
    // ── Signatures (IDA-style byte patterns) ──
    // ClientInstance::update — hooked to capture the live ClientInstance pointer.
    addSignature("ClientInstance::update",
        "48 89 5c 24 ? 48 89 74 24 ? 55 57 41 56 48 8d 6c 24 ? 48 81 ec ? ? ? ? 48 8b f1 e8 ? ? ? ? 48 8b d8");

    // ClientInstance::getLocalPlayerIndex — the virtual-call site whose
    // displacement encodes getLocalPlayer's vtable index (decoded in GameSDK).
    addSignature("ClientInstance::getLocalPlayerIndex",
        "49 8B 00 49 8B C8 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 48 85 C0 0F 84 ? ? ? ? 48 8B C8");

    // Options::getGamma — resolved for the Fullbright module.
    addSignature("Options::getGamma",
        "48 83 EC 28 48 8B 01 48 8D 54 24 30 41 B8 36 00 00 00");

    // ItemStack::getMaxDamage — lets HUDs show real durability bars.
    addSignature("ItemStack::getMaxDamage",
        "48 83 EC ? 48 8B 51 ? 33 C0 48 85 D2 74 ? 48 39 02 0F 95 C1");

    // Inventory::addItem — reference container-manipulation entry point.
    addSignature("Inventory::addItem",
        "48 89 74 24 ? 57 48 83 EC ? 48 8B 81 ? ? ? ? 48 8B F2 48 8B F9 48 85 C0 74 ? 80 B8");

    // GameMode::attack — hooked for attack/reach instrumentation (Hit Ping,
    // Reach Display). Detour shape is (GameMode*, Actor*, bool) on >= 1.21.50.
    // Last defined for 1.21.90, carried to 1.26.
    addSignature("GameMode::attack",
        "48 89 ? ? ? 48 89 ? ? ? 48 89 ? ? ? 55 41 ? 41 ? 41 ? 41 ? 48 8D ? ? ? ? ? ? 48 81 EC ? ? ? ? 48 8B ? ? ? ? ? 48 33 ? 48 89 ? ? ? ? ? 45 0F ? ? 4C 8B ? 48 8B ? 45 33 ? 44 89");

    // RakPeer::GetAveragePing — hooked to cache the live server RTT whenever the
    // game queries it. Last defined for 1.21.130, carried to 1.26.
    addSignature("RakPeer::GetAveragePing",
        "48 8B C4 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? 0F 10 4A ? 4C 8B 1A 4C 3B 1D ? ? ? ? 0F 10 42 ? 48 89 58 ? 48 8B D9 0F 10 52 ? 0F 10 5A ? 0F 10 62 ? 0F 10 6A ? 0F 29 70 ? 0F 10 72 ? 0F 29 78 ? 0F B7 82 ? ? ? ? 0F 10 BA ? ? ? ? 66 89 45 ? 0F B7 82 ? ? ? ? 66 89 45 ? 0F 11 4C 24 ? 74 ? 44 8B 49");

    // Camera bob matrix builder ("BobHurt") — hooked so view-bobbing modules can
    // inject a hand/camera translation. (void* self, glm::mat4* m). 1.21.120+.
    addSignature("BobHurt",
        "48 89 5C 24 ? 57 48 81 EC ? ? ? ? 0F 29 7C 24");

    // Minimal View Bobbing patch site — NOPing the 6-byte indirect call removes
    // the bob contribution entirely. Last defined for 1.21.110, carried to 1.26.
    addSignature("MinimalViewBobbing",
        "FF 15 ? ? ? ? 80 7C 24 ? ? 0F 84 ? ? ? ? F3 0F 10 4C 24 ? 0F 29 B4 24");

    // Server "show coordinates" gamerule check: `cmp byte [rax+x],y / setne al`.
    // Force Coords patches the setne into `mov al,1`. 1.20.30, carried forward.
    addSignature("ForceCoordsOption",
        "80 78 ? ? 0F 95 C0 48 8B 5C 24");

    // AppPlatform::readAssetFile — hooked by Material Bin Loader to substitute
    // *.material.bin contents with user-provided shader packs. 1.21.120+.
    addSignature("AppPlatform::readAssetFile",
        "48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 48 8B F2 48 89 55");

    // LocalPlayer::applyTurnDelta(Vec2<float>& {pitch, yaw}) — hooked for its
    // trampoline so Snap Look can apply camera turns on the game thread. 1.26.
    addSignature("LocalPlayer::applyTurnDelta",
        "48 8b c4 48 89 58 ? 48 89 70 ? 48 89 78 ? 55 41 54 41 55 41 56 41 57 48 8d 68 ? 48 81 ec ? ? ? ? 0f 29 70 ? 0f 29 78 ? 44 0f 29 40 ? 44 0f 29 48 ? 44 0f 29 50 ? 44 0f 29 98 ? ? ? ? 48 8b 05 ? ? ? ? 48 33 c4 48 89 45 ? 4c 8b ea");

    // Time-of-day computation (`time % 24000` helper) — hooked read-only so the
    // Day Counter can derive the absolute world time. 1.21.130, carried to 1.26.
    addSignature("TimeChanger",
        "44 8B C2 B8 ? ? ? ? F7 EA");

    // Level::getRuntimeActorList() -> std::vector<Actor*> — the game's own
    // thread-safe actor enumerator, called for entity Hitboxes. (Iterating the
    // EnTT registry directly races the server thread on 1.26 and crashes, so we
    // use the engine function exactly as Flarial does.) Effective 1.21.110→1.26.
    addSignature("Level::getRuntimeActorList",
        "48 89 5C 24 ? 55 56 57 48 83 EC ? 48 8B F2 48 89 54 24 ? 33 D2");

    // ── Offsets ──
    // Carried from 1.21.13x and NOT overridden by init260 → valid for 1.26.x.
    addOffset("ClientInstance::minecraftGame", 0x1A0);
    addOffset("ClientInstance::guiData",       0x648);
    addOffset("ClientInstance::levelRenderer", 0x1B8);
    addOffset("ClientInstance::camera",        0x358);
    addOffset("ClientInstance::packetSender",  0x1C8);

    // Overridden by init260 for 1.26.x.
    addOffset("MinecraftGame::gameRenderer",   0xD70);   // was 0xD30 @ 1.21.13x
    addOffset("MinecraftGame::textureGroup",   0x7A8);
    addOffset("Block::blockType",              0x60);    // was 0x78 @ 1.21.13x
    addOffset("BlockLegacy::name",             0xA8);
    addOffset("UIControl::children",           0x98);

    // Carried forward (not overridden by init260).
    addOffset("Item::blockType",          0x178);
    addOffset("Level::itemRegistry",      0x198);

    // Player container / inventory offsets. These are the ones that move most
    // often between builds; treat as build-specific until reconfirmed.
    addOffset("Player::supplies",         0x9B8);   // PlayerInventory*
    addOffset("PlayerInventory::container", 0x70);  // SimpleContainer (hotbar+main)
    addOffset("PlayerInventory::selectedSlot", 0x10);
    addOffset("Actor::armorContainer",    0x1670);
    addOffset("Actor::position",          0x44);

    // GameMode -> owning Player (stable since 1.20.30).
    addOffset("Gamemode::player",         0x8);

    // ── World-to-screen projection chain (entity Hitboxes) ──
    // GameRenderer holds the per-frame camera matrices at fixed slots.
    addOffset("GameRenderer::viewMatrix", 0x358);
    addOffset("GameRenderer::projMatrix", 0x3D8);
    // ClientInstance::getLevelRenderer is a *virtual* call on >= 1.21.120; this
    // is the vtable index (not a byte offset).
    addOffset("ClientInstance::getLevelRendererVIndex", 187);
    addOffset("LevelRenderer::levelRendererPlayer", 0x430);
    addOffset("LevelRendererPlayer::cameraPos",      0x704);
    // GuiData carries the render-target pixel size used for the NDC→screen map.
    addOffset("GuiData::screenSize",      0x40);
    // Actor -> owning Level (carried from 1.21.50, not overridden through 1.26).
    addOffset("Actor::level",             0x1D8);
}

} // namespace glacier::memory
