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
}

} // namespace glacier::memory
