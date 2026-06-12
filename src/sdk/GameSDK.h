#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "../memory/Memory.h"

// Minimal Minecraft: Bedrock SDK surface. Every build-specific address/offset is
// looked up by name from the SignatureManager, so this file contains only the
// resolution logic, never a hardcoded number.
//
// Patterns adapted from Flarial (dll-oss) and Latite:
//   • ClientInstance is captured live from a hook on ClientInstance::update
//     rather than chased through a global — robust across reorders.
//   • getLocalPlayer's vtable index is *decoded from a signature* and invoked
//     via virtual dispatch (Flarial's getLocalPlayerIndex technique).
//   • Fullbright drives a hook on Options::getGamma instead of poking a field.
namespace glacier::sdk {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// A single item slot read from the player's inventory/armor containers.
struct ItemStack {
    std::string name;          // display name (empty until Item registry is wired)
    int  count         = 0;
    int  durability    = 0;
    int  maxDurability = 0;    // 0 == not damageable
    bool enchanted     = false;
    bool valid         = false;
};

enum class ArmorSlot { Helmet = 0, Chestplate = 1, Leggings = 2, Boots = 3 };

// One projected box corner in render-target pixel space. `valid` is false when
// the corner is behind the camera or off-screen (the web layer skips edges that
// touch an invalid corner).
struct ScreenPoint {
    float x = 0, y = 0;
    bool  valid = false;
};

// A single entity's hitbox projected to screen: 8 cuboid corners + a 2-point
// "look line" (eye → facing). Corner order is the unit-cube convention used by
// the web renderer's edge table (see main.js BOX_EDGES).
struct ScreenBox {
    ScreenPoint corners[8];
    ScreenPoint lookFrom;
    ScreenPoint lookTo;
    bool        selected = false;   // entity under the crosshair
};

// One melee attack observed by the GameMode::attack hook. `seq` increments per
// event so HUD modules can cheaply detect "a new hit happened" without locks.
struct AttackInfo {
    Vec3          targetPos{};
    float         distance = 0.0f;   // attacker eye-line approx (feet-to-feet)
    std::uint64_t timeMs   = 0;      // GetTickCount64 at the moment of the hit
    std::uint32_t seq      = 0;
    bool          valid    = false;
};

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
    // Returns false if a required signature is missing (caller aborts attach).
    bool resolve();
    bool resolved() const { return m_resolved; }

    // Installs the SDK's own hooks (ClientInstance capture + gamma override).
    // Must run after the HookManager is initialized.
    void installHooks();

    // Set by the ClientInstance::update detour every tick.
    void setClientInstance(void* instance) { m_clientInstance.store(instance); }
    ClientInstance* clientInstance() const;
    LocalPlayer*    localPlayer() const;

    std::optional<Vec3> playerPosition() const;

    // Fullbright: while override >= 0, the getGamma hook returns it. -1 = off.
    void setGammaOverride(float gamma);

    // HUD data sources. Return empty/invalid stacks when unresolvable.
    std::array<ItemStack, 4> armor() const;
    std::vector<ItemStack>   inventory() const;
    ItemStack                heldItem() const;

    // ── Attack instrumentation (GameMode::attack hook) ──
    // Outgoing: the local player hit something. Incoming: something hit the
    // local player (only observable when the attacker is client-simulated).
    AttackInfo lastAttack() const;
    AttackInfo lastIncomingHit() const;
    void       recordAttack(void* gameMode, void* target);   // called by the hook

    // ── Network latency (RakPeer::GetAveragePing hook cache) ──
    // Milliseconds, or -1 while unknown / stale.
    int  ping() const;
    void cachePing(int ms);

    // ── World time (TimeChanger hook cache) ──
    // Absolute world time in ticks; nullopt while unknown. Day = ticks / 24000.
    std::optional<int> worldTime() const;
    void cacheWorldTime(int ticks);

    // ── View-bobbing override (BobHurt hook) ──
    // Non-zero translation is injected into the bob matrix each frame; (0,0)
    // restores vanilla. Used by Java View Bobbing.
    void setBobTranslation(float x, float y);

    // ── Camera turns (LocalPlayer::applyTurnDelta trampoline) ──
    // Queues a {pitch, yaw} turn in degrees; applied on the next game tick on
    // the game thread (Snap Look). No-op when the signature didn't resolve.
    void queueTurn(float yawDeg, float pitchDeg);
    void applyQueuedTurn();   // drains the queue; called by the update hook
    bool canTurn() const;

    // ── Entity hitbox projection (Hitboxes) ──
    // Enumerates nearby actors via the game's own getRuntimeActorList and
    // projects each one's AABB (and look line) to screen pixels. Runs entirely
    // on the render thread (live game pointers). Returns empty when the world,
    // matrices, or the actor-list signature aren't available — never throws.
    // `maxDistance` culls far actors; `lookLineLen` is the facing-ray length in
    // blocks (0 disables the look line).
    std::vector<ScreenBox> projectHitboxes(float maxDistance, float lookLineLen) const;

    // World→screen for a single point, in render-target pixels. nullopt when the
    // point is behind the camera, off-screen, or the matrices aren't ready.
    std::optional<std::pair<float, float>> worldToScreen(const Vec3& world) const;

    // ── Material redirect (AppPlatform::readAssetFile hook) ──
    // While enabled, *.material.bin reads are answered from `materials/` next
    // to the DLL when a same-named replacement file exists.
    void setMaterialRedirect(bool enabled);
    static std::string materialsDir();

private:
    GameSDK() = default;

    ItemStack readSlot(std::uintptr_t containerBase, int index) const;

    // Camera render origin for world→screen (LevelRenderer cameraPos, with a
    // local-player-position fallback).
    Vec3 cameraOrigin() const;

    bool m_resolved = false;

    // Captured live by the update hook.
    std::atomic<void*> m_clientInstance{ nullptr };

    // Decoded once in resolve().
    int m_localPlayerVIndex = -1;

    // Attack events — written from the game thread, read by HUD serializers.
    mutable std::mutex m_attackMutex;
    AttackInfo m_lastAttack{};
    AttackInfo m_lastIncoming{};
    std::uint32_t m_attackSeq = 0;

    // Ping / time caches with freshness stamps.
    std::atomic<int>           m_ping{ -1 };
    std::atomic<std::uint64_t> m_pingStamp{ 0 };
    std::atomic<int>           m_worldTime{ -1 };
    std::atomic<std::uint64_t> m_worldTimeStamp{ 0 };
};

} // namespace glacier::sdk
