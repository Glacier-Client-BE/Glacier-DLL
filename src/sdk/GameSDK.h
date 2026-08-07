#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

#include "../memory/Memory.h"

// Minimal Minecraft: Bedrock SDK surface.
//
// Every build-specific address and offset is looked up by name from the
// SignatureManager, so this file contains resolution logic and never a
// hardcoded number. That separation is what keeps all third-party-derived data
// confined to src/memory/Signatures.cpp (see docs/acknowledgements.md).
//
// Phase 1 scope is deliberately narrow: capture ClientInstance, reach
// LocalPlayer, and drive the gamma override that Fullbright needs. Inventory,
// armor, attack/ping/time instrumentation, and world→screen projection arrive
// in Phase 3 alongside the modules that consume them — each pulls in more
// offsets, and an offset with no consumer is an unverifiable liability.
namespace glacier::sdk {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// One item slot read from a container. `valid` is false for an empty slot, so
// callers never have to distinguish "air" from "couldn't read".
struct ItemStack {
    int  count         = 0;
    int  damage        = 0;
    int  maxDurability = 0;   // 0 == not damageable
    bool valid         = false;

    // Remaining durability as 0..1, or 1 when the item isn't damageable.
    float durabilityFraction() const {
        if (maxDurability <= 0) return 1.0f;
        const float left = static_cast<float>(maxDurability - damage)
                         / static_cast<float>(maxDurability);
        return left < 0.0f ? 0.0f : (left > 1.0f ? 1.0f : left);
    }
};

enum class ArmorSlot { Helmet = 0, Chestplate = 1, Leggings = 2, Boots = 3 };

// Opaque game objects — only ever held as pointers.
class ClientInstance;
class LocalPlayer;

class GameSDK {
public:
    static GameSDK& get() {
        static GameSDK instance;
        return instance;
    }

    // Seeds + scans signatures and decodes the getLocalPlayer vtable index.
    // Returns false if a required signature is missing, in which case the
    // caller must abort the attach: continuing without these pointers means
    // dereferencing garbage, and a clean refusal is a far better failure mode.
    bool resolve();
    bool resolved() const { return m_resolved; }

    // Installs the SDK's own hooks (ClientInstance capture + gamma override).
    // Must run after HookManager::initialize().
    void installHooks();

    // Set by the ClientInstance::update detour every tick.
    void setClientInstance(void* instance) {
        m_clientInstance.store(instance, std::memory_order_relaxed);
    }
    ClientInstance* clientInstance() const;
    LocalPlayer*    localPlayer() const;

    std::optional<Vec3> playerPosition() const;

    // Fullbright: while the override is >= 0 the getGamma hook returns it
    // instead of the player's real brightness. -1 restores vanilla behaviour.
    void setGammaOverride(float gamma);

    // False when Options::getGamma didn't resolve, so Fullbright can tell the
    // user why it is doing nothing instead of silently failing.
    bool gammaHookActive() const;

    // ── Instrumentation caches ──
    // Each is fed by a hook and read by exactly one HUD. All return a "no data"
    // sentinel rather than a stale value when the hook never fired, so a
    // missing signature shows as "--" instead of a plausible wrong number.

    // Server round-trip time in ms, or -1 when unknown/stale.
    int  ping() const;
    void cachePing(int ms);

    // Absolute world time in ticks; nullopt while unknown. Day = ticks / 24000.
    std::optional<int> worldTime() const;
    void cacheWorldTime(int ticks);

    // Distance in blocks of the most recent melee hit, or nullopt if none has
    // been observed. `ageMs` reports how long ago, so a HUD can fade it out.
    std::optional<float> lastAttackDistance(std::uint64_t* ageMs = nullptr) const;
    void recordAttack(void* gameMode, void* target);   // called by the hook

    // ── Containers ──
    // All return empty/invalid data rather than throwing when the player isn't
    // in a world or the offsets didn't resolve, so HUD widgets can call them
    // unconditionally every frame.
    std::array<ItemStack, 4> armor() const;
    ItemStack                heldItem() const;

private:
    GameSDK() = default;

    ItemStack readSlot(std::uintptr_t containerBase, int index) const;

    bool m_resolved = false;

    // Captured live by the update hook, read from the logic + render threads.
    std::atomic<void*> m_clientInstance{ nullptr };

    // Decoded once in resolve() from the getLocalPlayerIndex call site.
    int m_localPlayerVIndex = -1;

    // Instrumentation caches. Written from game threads, read from the render
    // thread — atomics rather than a mutex, since each is a single scalar and
    // the render thread must never block behind a game thread.
    std::atomic<int>           m_ping{ -1 };
    std::atomic<std::uint64_t> m_pingStamp{ 0 };
    std::atomic<int>           m_worldTime{ -1 };
    std::atomic<float>         m_attackDistance{ -1.0f };
    std::atomic<std::uint64_t> m_attackStamp{ 0 };
};

} // namespace glacier::sdk
