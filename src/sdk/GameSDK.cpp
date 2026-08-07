#include "GameSDK.h"

#include "../hook/HookManager.h"
#include "../memory/SignatureManager.h"
#include "../util/Logger.h"

#include <cmath>
#include <Windows.h>

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

// ── GameMode::attack hook (Reach display) ──
// Strictly observational: record, then call through unmodified. Glacier does
// not alter attacks — see the scope boundary in README.
using AttackFn = void(__fastcall*)(void*, void*, bool);
AttackFn o_attack = nullptr;

void __fastcall hkAttack(void* gameMode, void* target, bool a3) {
    GameSDK::get().recordAttack(gameMode, target);
    o_attack(gameMode, target, a3);
}

// ── RakPeer::GetAveragePing hook ──
// Caches whatever the game asks for. We never call this ourselves: reaching
// into RakNet off the network thread isn't safe.
using PingFn = std::int32_t(__fastcall*)(void*, void*);
PingFn o_getAveragePing = nullptr;

std::int32_t __fastcall hkGetAveragePing(void* peer, void* addressOrGuid) {
    const auto result = o_getAveragePing(peer, addressOrGuid);
    if (result >= 0) {
        GameSDK::get().cachePing(static_cast<int>(result));
    }
    return result;
}

// ── Time-of-day hook (Day Counter) ──
// The game calls this helper with the raw tick count to reduce it mod 24000.
// We only read the input, then call through.
using TimeFn = int(__fastcall*)(void*, std::int64_t);
TimeFn o_timeOfDay = nullptr;

int __fastcall hkTimeOfDay(void* a1, std::int64_t rawTicks) {
    if (rawTicks >= 0 && rawTicks < 0x7FFFFFFF) {
        GameSDK::get().cacheWorldTime(static_cast<int>(rawTicks));
    }
    return o_timeOfDay(a1, rawTicks);
}

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

    // Instrumentation hooks. Every one of these is optional and drives exactly
    // one HUD, which displays "--" when its hook never installed.
    if (const auto addr = sigs.sig("GameMode::attack")) {
        hooks.create<AttackFn>("GameMode::attack",
                               reinterpret_cast<void*>(addr), &hkAttack, &o_attack);
    } else {
        LOG_WARN("GameMode::attack not resolved — Reach display will show no data");
    }
    if (const auto addr = sigs.sig("RakPeer::GetAveragePing")) {
        hooks.create<PingFn>("RakPeer::GetAveragePing",
                             reinterpret_cast<void*>(addr), &hkGetAveragePing, &o_getAveragePing);
    } else {
        LOG_WARN("RakPeer::GetAveragePing not resolved — Ping display unavailable");
    }
    if (const auto addr = sigs.sig("TimeChanger")) {
        hooks.create<TimeFn>("TimeChanger",
                             reinterpret_cast<void*>(addr), &hkTimeOfDay, &o_timeOfDay);
    } else {
        LOG_WARN("TimeChanger not resolved — Day Counter will show no data");
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

// ─── Instrumentation caches ──────────────────────────────────────────────────

void GameSDK::cachePing(int ms) {
    m_ping.store(ms, std::memory_order_relaxed);
    m_pingStamp.store(GetTickCount64(), std::memory_order_relaxed);
}

int GameSDK::ping() const {
    // The game stops querying ping in single-player and on the main menu, so an
    // old value would sit on screen indefinitely. Expire it.
    const auto stamp = m_pingStamp.load(std::memory_order_relaxed);
    if (stamp == 0 || GetTickCount64() - stamp > 5000) return -1;
    return m_ping.load(std::memory_order_relaxed);
}

void GameSDK::cacheWorldTime(int ticks) {
    m_worldTime.store(ticks, std::memory_order_relaxed);
}

std::optional<int> GameSDK::worldTime() const {
    const int t = m_worldTime.load(std::memory_order_relaxed);
    if (t < 0) return std::nullopt;
    return t;
}

void GameSDK::recordAttack(void* gameMode, void* target) {
    if (!gameMode || !target) return;

    auto& sigs = SignatureManager::get();
    const auto posOff = sigs.offset("Actor::position");
    const auto playerOff = sigs.offset("Gamemode::player");
    if (posOff == 0) return;

    // The attacker is the GameMode's owning player rather than localPlayer():
    // reaching through the object we were handed avoids a virtual call on a
    // pointer that may not be the local player at all.
    void* attacker = memory::memberAt<void*>(gameMode, playerOff);
    if (!attacker) return;

    const Vec3 from = memory::memberAt<Vec3>(attacker, posOff);
    const Vec3 to   = memory::memberAt<Vec3>(target, posOff);

    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Reject implausible values: a bad offset produces huge or NaN distances,
    // and showing "1.4e+27 blocks" is worse than showing nothing.
    if (!std::isfinite(distance) || distance < 0.0f || distance > 64.0f) return;

    m_attackDistance.store(distance, std::memory_order_relaxed);
    m_attackStamp.store(GetTickCount64(), std::memory_order_relaxed);
}

std::optional<float> GameSDK::lastAttackDistance(std::uint64_t* ageMs) const {
    const auto stamp = m_attackStamp.load(std::memory_order_relaxed);
    if (stamp == 0) return std::nullopt;

    if (ageMs) *ageMs = GetTickCount64() - stamp;
    return m_attackDistance.load(std::memory_order_relaxed);
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
