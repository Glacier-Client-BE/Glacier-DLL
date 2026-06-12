#include "GameSDK.h"

#include "../hook/HookManager.h"
#include "../memory/SignatureManager.h"
#include "../util/Logger.h"

namespace glacier::sdk {

using memory::SignatureManager;

namespace {

// ── ClientInstance::update capture hook ──
// Captures the live ClientInstance pointer each tick (Flarial's pattern), which
// avoids relying on a global that may move between builds.
using UpdateFn = void(__fastcall*)(void*);
UpdateFn o_update = nullptr;

void __fastcall hkUpdate(void* self) {
    GameSDK::get().setClientInstance(self);
    o_update(self);
}

// ── Options::getGamma hook (Fullbright) ──
// Returns the override gamma while Fullbright is on, the game's real value
// otherwise. Cleaner and safer than writing the Options field directly.
using GammaFn = float(__fastcall*)(void*);
GammaFn o_getGamma = nullptr;
std::atomic<float> s_gammaOverride{ -1.0f };

float __fastcall hkGetGamma(void* self) {
    const float real = o_getGamma(self);
    const float ov = s_gammaOverride.load(std::memory_order_relaxed);
    return ov >= 0.0f ? ov : real;
}

// ItemStack::getMaxDamage(ItemStack* this) -> int
using GetMaxDamageFn = int(__fastcall*)(void*);
GetMaxDamageFn p_getMaxDamage = nullptr;

} // namespace

bool GameSDK::resolve() {
    if (m_resolved) return true;

    auto& sigs = SignatureManager::get();
    sigs.seedBedrock();
    sigs.scanAll();

    // Decode getLocalPlayer's vtable index from the call-site signature.
    // The instruction at sig+9 holds the 4-byte vtable byte-offset; /8 = index.
    if (const auto idxSite = sigs.sig("ClientInstance::getLocalPlayerIndex")) {
        m_localPlayerVIndex = *reinterpret_cast<const std::int32_t*>(idxSite + 9) / 8;
        LOG_INFO("getLocalPlayer vtable index = {}", m_localPlayerVIndex);
    } else {
        LOG_ERROR("getLocalPlayerIndex signature missing — aborting attach");
        return false;
    }

    p_getMaxDamage = reinterpret_cast<GetMaxDamageFn>(sigs.sig("ItemStack::getMaxDamage"));

    // Require the capture hook target; without it we can never reach the player.
    if (!sigs.sig("ClientInstance::update")) {
        LOG_ERROR("ClientInstance::update signature missing — aborting attach");
        return false;
    }

    m_resolved = true;
    return true;
}

void GameSDK::installHooks() {
    if (!m_resolved) return;
    auto& sigs = SignatureManager::get();
    auto& hooks = HookManager::get();

    if (const auto addr = sigs.sig("ClientInstance::update")) {
        hooks.create<UpdateFn>("ClientInstance::update",
                               reinterpret_cast<void*>(addr), &hkUpdate, &o_update);
    }
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
    return memory::callVirtualI<LocalPlayer*>(static_cast<std::uint32_t>(m_localPlayerVIndex), ci);
}

std::optional<Vec3> GameSDK::playerPosition() const {
    auto* lp = localPlayer();
    if (!lp) return std::nullopt;
    const auto off = SignatureManager::get().offset("Actor::position");
    return memory::memberAt<Vec3>(lp, off);
}

void GameSDK::setGammaOverride(float gamma) {
    s_gammaOverride.store(gamma, std::memory_order_relaxed);
}

ItemStack GameSDK::readSlot(std::uintptr_t containerBase, int index) const {
    ItemStack stack;
    if (!containerBase) return stack;

    // A Bedrock container holds a contiguous std::vector<ItemStack>. Read the
    // vector's [begin,end) pointers, bounds-check, then index into it.
    // ItemStack layout (per build): [+0x08] Item*, [+0x20] count, [+0x22] aux.
    constexpr std::ptrdiff_t kStackStride = 0x88;

    const auto data = *reinterpret_cast<std::uintptr_t*>(containerBase + 0x00);
    const auto end  = *reinterpret_cast<std::uintptr_t*>(containerBase + 0x08);
    if (!data || data >= end) return stack;

    const auto count = static_cast<int>((end - data) / kStackStride);
    if (index < 0 || index >= count) return stack;

    const auto slot = data + static_cast<std::uintptr_t>(index) * kStackStride;
    const auto item = *reinterpret_cast<std::uintptr_t*>(slot + 0x08);
    if (!item) return stack;   // air

    stack.valid      = true;
    stack.count      = *reinterpret_cast<std::uint8_t*>(slot + 0x20);
    stack.durability = *reinterpret_cast<std::int16_t*>(slot + 0x22);
    // Real max durability via the resolved getMaxDamage — drives HUD bars.
    if (p_getMaxDamage) {
        stack.maxDurability = p_getMaxDamage(reinterpret_cast<void*>(slot));
    }
    return stack;
}

std::array<ItemStack, 4> GameSDK::armor() const {
    std::array<ItemStack, 4> out{};
    auto* lp = localPlayer();
    if (!lp) return out;

    const auto base = reinterpret_cast<std::uintptr_t>(lp)
                    + SignatureManager::get().offset("Actor::armorContainer");
    for (int i = 0; i < 4; ++i) out[i] = readSlot(base, i);
    return out;
}

std::vector<ItemStack> GameSDK::inventory() const {
    std::vector<ItemStack> out;
    auto* lp = localPlayer();
    if (!lp) return out;

    auto& sigs = SignatureManager::get();
    const auto supplies = memory::memberAt<std::uintptr_t>(lp, sigs.offset("Player::supplies"));
    if (!supplies) return out;

    const auto container = supplies + sigs.offset("PlayerInventory::container");
    out.reserve(36);
    for (int i = 0; i < 36; ++i) out.push_back(readSlot(container, i));
    return out;
}

ItemStack GameSDK::heldItem() const {
    auto* lp = localPlayer();
    if (!lp) return {};

    auto& sigs = SignatureManager::get();
    const auto supplies = memory::memberAt<std::uintptr_t>(lp, sigs.offset("Player::supplies"));
    if (!supplies) return {};

    const auto selected = memory::memberAt<int>(
        reinterpret_cast<void*>(supplies), sigs.offset("PlayerInventory::selectedSlot"));
    return readSlot(supplies + sigs.offset("PlayerInventory::container"), selected);
}

} // namespace glacier::sdk
