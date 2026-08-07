#include "GameSDK.h"

#include "../hook/HookManager.h"
#include "../memory/SignatureManager.h"
#include "../util/Logger.h"

namespace glacier::sdk {

using memory::SignatureManager;

namespace {

// ── ClientInstance::update capture hook ──
// Captures the live ClientInstance pointer each tick. Preferred over chasing a
// global: the pointer is handed to us by the game itself, so it stays correct
// across builds that move or restructure their globals.
using UpdateFn = void(__fastcall*)(void*);
UpdateFn o_update = nullptr;

void __fastcall hkUpdate(void* self) {
    GameSDK::get().setClientInstance(self);
    o_update(self);
}

// ── Options::getGamma hook (Fullbright) ──
// Returns the override while Fullbright is on and the game's real value
// otherwise. Hooking the accessor rather than writing the Options field means
// the player's saved brightness is never actually modified — nothing to restore
// if the client is unloaded abruptly.
using GammaFn = float(__fastcall*)(void*);
GammaFn o_getGamma = nullptr;
std::atomic<float> s_gammaOverride{ -1.0f };

float __fastcall hkGetGamma(void* self) {
    const float real = o_getGamma(self);
    const float ov = s_gammaOverride.load(std::memory_order_relaxed);
    return ov >= 0.0f ? ov : real;
}

// ItemStack::getMaxDamage(ItemStack*) -> int. Called directly, not hooked.
using GetMaxDamageFn = int(__fastcall*)(void*);
GetMaxDamageFn p_getMaxDamage = nullptr;

} // namespace

bool GameSDK::resolve() {
    if (m_resolved) return true;

    auto& sigs = SignatureManager::get();
    sigs.seedBedrock();
    sigs.scanAll();

    // Decode getLocalPlayer's vtable index from the call-site signature: the
    // 4-byte displacement at sig+9 is the byte offset into the vtable, and
    // entries are pointer-sized, so /8 yields the index.
    const auto idxSite = sigs.sig("ClientInstance::getLocalPlayerIndex");
    if (!idxSite) {
        LOG_ERROR("ClientInstance::getLocalPlayerIndex not resolved — aborting attach");
        return false;
    }
    m_localPlayerVIndex = *reinterpret_cast<const std::int32_t*>(idxSite + 9) / 8;

    // A nonsense index means the signature matched the wrong site (or the call
    // shape changed); calling through it would jump to an arbitrary address.
    if (m_localPlayerVIndex < 0 || m_localPlayerVIndex > 1024) {
        LOG_ERROR("decoded implausible getLocalPlayer vtable index ({}) — aborting attach",
                  m_localPlayerVIndex);
        return false;
    }
    LOG_INFO("getLocalPlayer vtable index = {}", m_localPlayerVIndex);

    // Optional: durability bars degrade to "no bar" without it.
    p_getMaxDamage = reinterpret_cast<GetMaxDamageFn>(sigs.sig("ItemStack::getMaxDamage"));

    // Without the capture hook target we can never reach the player at all.
    if (!sigs.sig("ClientInstance::update")) {
        LOG_ERROR("ClientInstance::update not resolved — aborting attach");
        return false;
    }

    m_resolved = true;
    return true;
}

void GameSDK::installHooks() {
    if (!m_resolved) return;

    auto& sigs  = SignatureManager::get();
    auto& hooks = HookManager::get();

    if (const auto addr = sigs.sig("ClientInstance::update")) {
        hooks.create<UpdateFn>("ClientInstance::update",
                               reinterpret_cast<void*>(addr), &hkUpdate, &o_update);
    }

    // Not attach-blocking: a missing gamma hook costs Fullbright, nothing else.
    if (const auto addr = sigs.sig("Options::getGamma")) {
        hooks.create<GammaFn>("Options::getGamma",
                              reinterpret_cast<void*>(addr), &hkGetGamma, &o_getGamma);
    } else {
        LOG_WARN("Options::getGamma not resolved — Fullbright will no-op");
    }
}

ClientInstance* GameSDK::clientInstance() const {
    return reinterpret_cast<ClientInstance*>(m_clientInstance.load(std::memory_order_relaxed));
}

LocalPlayer* GameSDK::localPlayer() const {
    void* ci = m_clientInstance.load(std::memory_order_relaxed);
    if (!ci || m_localPlayerVIndex < 0) return nullptr;
    return memory::callVirtualI<LocalPlayer*>(
        static_cast<std::uint32_t>(m_localPlayerVIndex), ci);
}

std::optional<Vec3> GameSDK::playerPosition() const {
    auto* lp = localPlayer();
    if (!lp) return std::nullopt;

    const auto off = SignatureManager::get().offset("Actor::position");
    if (off == 0) return std::nullopt;

    return memory::memberAt<Vec3>(lp, off);
}

void GameSDK::setGammaOverride(float gamma) {
    s_gammaOverride.store(gamma, std::memory_order_relaxed);
}

bool GameSDK::gammaHookActive() const {
    return o_getGamma != nullptr;
}

// ─── Containers ──────────────────────────────────────────────────────────────

ItemStack GameSDK::readSlot(std::uintptr_t containerBase, int index) const {
    ItemStack stack;
    if (!containerBase || index < 0) return stack;

    auto& sigs = SignatureManager::get();
    const auto stride = sigs.offset("ItemStack::stride");
    if (stride <= 0) return stack;

    // A Bedrock container is a contiguous std::vector<ItemStack>: read the
    // [begin, end) pointers, bounds-check, then index by stride. Every offset
    // comes from the registry — see the containment rule in Signatures.cpp.
    const auto begin = *reinterpret_cast<std::uintptr_t*>(
        containerBase + sigs.offset("Container::begin"));
    const auto end = *reinterpret_cast<std::uintptr_t*>(
        containerBase + sigs.offset("Container::end"));
    if (!begin || begin >= end) return stack;

    const auto slots = static_cast<std::ptrdiff_t>((end - begin) / static_cast<std::uintptr_t>(stride));
    if (index >= slots) return stack;

    const auto slot = begin + static_cast<std::uintptr_t>(index) * static_cast<std::uintptr_t>(stride);
    const auto item = *reinterpret_cast<std::uintptr_t*>(slot + sigs.offset("ItemStack::item"));
    if (!item) return stack;   // empty slot (air)

    stack.valid  = true;
    stack.count  = *reinterpret_cast<std::uint8_t*>(slot + sigs.offset("ItemStack::count"));
    stack.damage = *reinterpret_cast<std::int16_t*>(slot + sigs.offset("ItemStack::auxValue"));
    if (p_getMaxDamage) {
        stack.maxDurability = p_getMaxDamage(reinterpret_cast<void*>(slot));
    }
    return stack;
}

std::array<ItemStack, 4> GameSDK::armor() const {
    std::array<ItemStack, 4> out{};
    auto* lp = localPlayer();
    if (!lp) return out;

    const auto off = SignatureManager::get().offset("Actor::armorContainer");
    if (off == 0) return out;

    const auto base = reinterpret_cast<std::uintptr_t>(lp) + off;
    for (int i = 0; i < 4; ++i) out[i] = readSlot(base, i);
    return out;
}

ItemStack GameSDK::heldItem() const {
    auto* lp = localPlayer();
    if (!lp) return {};

    auto& sigs = SignatureManager::get();
    const auto suppliesOff = sigs.offset("Player::supplies");
    if (suppliesOff == 0) return {};

    const auto supplies = memory::memberAt<std::uintptr_t>(lp, suppliesOff);
    if (!supplies) return {};

    const auto selected = memory::memberAt<int>(
        reinterpret_cast<void*>(supplies), sigs.offset("PlayerInventory::selectedSlot"));
    return readSlot(supplies + sigs.offset("PlayerInventory::container"), selected);
}

} // namespace glacier::sdk
