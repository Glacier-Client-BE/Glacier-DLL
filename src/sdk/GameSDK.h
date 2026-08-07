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
// ClientInstance is reached by walking globals rather than by hooking an update
// function: the chain needs no hook and works before the first tick. Entity
// position lives in an ECS component, and inventory slots are read through a
// virtual getItem rather than by walking the backing vector by hand — both are
// what the current build actually requires.
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

    // Seeds + scans signatures and decodes the root global. Returns false if a
    // required signature is missing, in which case the caller must abort the
    // attach: continuing without these pointers means dereferencing garbage,
    // and a clean refusal is a far better failure mode.
    bool resolve();
    bool resolved() const { return m_resolved; }

    // Installs the SDK's optional instrumentation hooks (gamma override, attack
    // observation, ping and world-time caches). Must run after
    // HookManager::initialize().
    void installHooks();

    // Walks the global object graph to the primary ClientInstance. Returns
    // nullptr whenever any link is missing (main menu, loading, disconnect),
    // which callers must treat as normal rather than exceptional.
    ClientInstance* clientInstance() const;
    LocalPlayer*    localPlayer() const;

    // True only when the player is actually in a world. The main menu, the
    // loading screen and the moment after a disconnect all report false.
    //
    // Glacier's overlay and menu are gated on this, the way Latite and Flarial
    // gate theirs: HUD widgets over the main menu are noise, and a menu that
    // opens where there is no player to configure anything for invites exactly
    // the kind of null-deref that looks like "the client crashes on startup".
    bool inGame() const { return localPlayer() != nullptr; }

    std::optional<Vec3> playerPosition() const;

    // Fullbright: while the override is >= 0 the getGamma hook returns it
    // instead of the player's real brightness. -1 restores vanilla behaviour.
    void setGammaOverride(float gamma);

    // False when Options::getGamma didn't resolve, so Fullbright can tell the
    // user why it is doing nothing instead of silently failing.
    bool gammaHookActive() const;

    // Drives the game's own cursor grab state. Releasing the cursor is what
    // actually pauses look and movement: Bedrock reads the mouse through
    // RawInput, so swallowing window messages never stopped the player from
    // moving behind the menu. Grabbing it again restores normal play.
    //
    // Reconciles the game's cursor state against whether Glacier's menu is
    // open. Call once per frame from a game thread (the Present hook).
    //
    // This is Latite's `ScreenManager::onUpdate` shape, and the repetition is
    // the whole point: **the game re-grabs the cursor on its own**, so calling
    // releaseCursor() once when the menu opens does not hold — which is why the
    // game kept accepting movement behind the menu. While the menu is open, any
    // frame that finds the cursor grabbed releases it again.
    //
    // Grabbing, by contrast, happens only on the open->closed edge. Never grab
    // just because the cursor is released: the game releases it for its own
    // screens (pause, inventory, chat), and re-grabbing there would fight the
    // game for control of its own UI.
    void applyCursorState(bool menuOpen);

    // Whether the game currently has the cursor captured for gameplay.
    bool cursorGrabbed() const;

    // True when both cursor signatures resolved. Independent of whether a
    // ClientInstance currently exists.
    bool cursorControlAvailable() const;

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
    void recordAttack(void* attacker, void* target);   // called by the hook

    // ── Containers ──
    // All return empty/invalid data rather than throwing when the player isn't
    // in a world or the offsets didn't resolve, so HUD widgets can call them
    // unconditionally every frame.
    // Armor is not readable on this build — see the note in armor(). Query
    // armorSupported() to render an honest "unavailable" rather than blanks.
    std::array<ItemStack, 4> armor() const;
    bool                     armorSupported() const;
    ItemStack                heldItem() const;

    // Raw game `ItemStack*` for a slot, or nullptr. These exist for the item
    // renderer, which has to hand the game back its own object rather than our
    // decoded copy.
    //
    // The pointer is only valid for the call frame that obtained it — the
    // containers behind it are the game's, and it must never be stored across
    // frames. `slot` is 0..3 (helmet -> boots) for armor, and 0..8 or -1 for
    // "currently selected" on the hotbar.
    void* rawArmorStack(int slot) const;
    void* rawHotbarStack(int slot) const;

    // 1 / the game's GUI scale, i.e. GUI units per screen pixel. Item drawing
    // needs it to convert Glacier's pixel layout into the coordinates the
    // game's UI renderer expects. Returns 0 when unavailable, which callers
    // must treat as "cannot draw" rather than substituting 1.
    float guiScaleFrac() const;

private:
    GameSDK() = default;

    ItemStack readStack(void* stackPtr) const;

    bool m_resolved = false;

    // Address of the global that roots the whole object graph, decoded once in
    // resolve() from the Platform_GameCore signature.
    std::uintptr_t m_gameCoreGlobal = 0;

    // Vtable index of ClientInstance::getLocalPlayer for the target build.
    int m_localPlayerVIndex = -1;

    // Instrumentation caches. Written from game threads, read from the render
    // thread — atomics rather than a mutex, since each is a single scalar and
    // the render thread must never block behind a game thread.
    // Previous menu state, so the grab fires once on close rather than every
    // frame. Only touched from the frame thread that calls applyCursorState.
    bool m_menuWasOpen = false;

    std::atomic<int>           m_ping{ -1 };
    std::atomic<std::uint64_t> m_pingStamp{ 0 };
    std::atomic<int>           m_worldTime{ -1 };
    std::atomic<float>         m_attackDistance{ -1.0f };
    std::atomic<std::uint64_t> m_attackStamp{ 0 };
};

} // namespace glacier::sdk
