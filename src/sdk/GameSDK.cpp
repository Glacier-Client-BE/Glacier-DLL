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

} // namespace glacier::sdk
