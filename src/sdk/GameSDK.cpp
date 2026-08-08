#include "GameSDK.h"

#include "../hook/HookManager.h"
#include "../memory/SignatureManager.h"
#include "EntityComponents.h"
#include "../util/CrashHandler.h"
#include "../util/Logger.h"

#include <cmath>
#include <map>
#include <memory>
#include <Windows.h>

namespace glacier::sdk {

using memory::SignatureManager;

namespace {

// ── Options::getGamma hook (Fullbright) ──
// Returns the override while Fullbright is on and the game's real value
// otherwise. Hooking the accessor rather than writing the Options field means
// the player's saved brightness is never modified — nothing to restore if the
// client is unloaded abruptly.
using GammaFn = float(__fastcall*)(void*);
GammaFn o_getGamma = nullptr;
std::atomic<float> s_gammaOverride{ -1.0f };

float __fastcall hkGetGamma(void* self) {
    const float real = o_getGamma(self);
    const float ov = s_gammaOverride.load(std::memory_order_relaxed);
    return ov >= 0.0f ? ov : real;
}

// ── Actor::attack hook (Reach display) ──
// Strictly observational: record, then call through unmodified. Glacier does
// not alter attacks — see the scope boundary in README. Unlike the previous
// GameMode::attack shape, `self` is the attacker directly.
using AttackFn = void(__fastcall*)(void*, void*, void*);
AttackFn o_attack = nullptr;

void __fastcall hkAttack(void* self, void* target, void* a3) {
    GameSDK::get().recordAttack(self, target);
    o_attack(self, target, a3);
}

// ── RakPeer::GetAveragePing hook ──
using PingFn = std::int32_t(__fastcall*)(void*, void*);
PingFn o_getAveragePing = nullptr;

std::int32_t __fastcall hkGetAveragePing(void* peer, void* addressOrGuid) {
    const auto result = o_getAveragePing(peer, addressOrGuid);
    if (result >= 0) {
        GameSDK::get().cachePing(static_cast<int>(result));
    }
    return result;
}

// ── Dimension::getTimeOfDay hook (Day Counter) ──
// Read-only: capture the raw tick count the game passes in, then call through.
using TimeFn = int(__fastcall*)(void*, int);
TimeFn o_getTimeOfDay = nullptr;

int __fastcall hkGetTimeOfDay(void* self, int rawTicks) {
    if (rawTicks >= 0) {
        GameSDK::get().cacheWorldTime(rawTicks);
    }
    return o_getTimeOfDay(self, rawTicks);
}

// ── ClientInstance::grabCursor / releaseCursor ──
// Called directly rather than hooked. These are the game's own "is the mouse
// captured by gameplay" toggle, which is the only thing that reliably stops
// look and movement while the menu is up.
using CursorFn = void(__fastcall*)(void*);
CursorFn s_grabCursor = nullptr;
CursorFn s_releaseCursor = nullptr;

// ── ItemStackBase::getDamageValue ──
// The real damage value. The raw auxValue field is not the damage for every
// item, so reading it as one produced durability numbers that were right for
// tools and meaningless for everything else.
using DamageFn = std::int16_t(__fastcall*)(void*);
DamageFn s_getDamageValue = nullptr;

// ── The ClientInstance map read ──
//
// MinecraftGame holds its ClientInstances in a std::map, which we reinterpret
// through our own std::map because both are MSVC STL. That works, and it is
// what every client in this space does — but it is a lock-free read of a
// container the game mutates, and the game mutates it exactly when a world is
// created or torn down. A find() that walks the tree mid-rebalance reads a
// half-written node and faults.
//
// That is "crashes shortly after loading a world" in one sentence, and no
// amount of null-checking prevents it: the pointers are non-null, just briefly
// inconsistent. The read is guarded instead, and a fault reports "no client
// instance" for that frame — which every caller already handles, because it is
// the same answer they get on the main menu.
void* readPrimaryInstanceUnguarded(void* mcGame, std::ptrdiff_t mapOffset) {
    using InstanceMap = std::map<std::uint8_t, std::shared_ptr<ClientInstance>>;
    const auto& instances = *reinterpret_cast<const InstanceMap*>(
        reinterpret_cast<std::uintptr_t>(mcGame) + static_cast<std::uintptr_t>(mapOffset));

    const auto it = instances.find(0);
    return it != instances.end() ? it->second.get() : nullptr;
}

// Separate function so the __try has nothing to unwind — MSVC rejects __try in
// a function that needs C++ object unwinding, and the map read above has
// plenty.
void* readPrimaryInstance(void* mcGame, std::ptrdiff_t mapOffset) {
    __try {
        return readPrimaryInstanceUnguarded(mcGame, mapOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// Reads a pointer field, tolerating a null base so a broken chain degrades to
// nullptr rather than faulting.
inline void* deref(void* base, std::ptrdiff_t offset) {
    if (!base) return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(base) + offset);
}

} // namespace

bool GameSDK::resolve() {
    if (m_resolved) return true;

    auto& sigs = SignatureManager::get();
    sigs.seedBedrock();
    sigs.scanAll();

    // The whole object graph hangs off this one global. Without it there is no
    // player, no world, and nothing worth attaching for.
    //
    // The signature entry carries its own deref (the pattern matches the `mov
    // [rip+disp], r15` that writes the global, and the displacement at +3 is
    // followed for us), so this is already the address of the global.
    m_gameCoreGlobal = sigs.sig("Platform_GameCore");
    if (!m_gameCoreGlobal) {
        LOG_ERROR("Platform_GameCore not resolved — aborting attach");
        return false;
    }

    m_localPlayerVIndex = static_cast<int>(sigs.offset("ClientInstance::getLocalPlayerVIndex"));
    if (m_localPlayerVIndex <= 0 || m_localPlayerVIndex > 1024) {
        LOG_ERROR("implausible getLocalPlayer vtable index ({}) — aborting attach",
                  m_localPlayerVIndex);
        return false;
    }

    // Optional: without these the menu still opens, it just can't pause the
    // game. Warn rather than abort, and say what is lost.
    s_grabCursor    = reinterpret_cast<CursorFn>(sigs.sig("ClientInstance::grabCursor"));
    s_releaseCursor = reinterpret_cast<CursorFn>(sigs.sig("ClientInstance::releaseCursor"));
    if (!s_grabCursor || !s_releaseCursor) {
        LOG_WARN("ClientInstance::grabCursor/releaseCursor not resolved — the game will "
                 "keep receiving movement and look input while the menu is open");
    } else {
        // Resolving is not the same as working — see applyCursorState for the
        // per-frame trace that actually confirms the call took effect.
        LOG_INFO("ClientInstance::grabCursor/releaseCursor resolved");
    }

    // Optional too. Without it, durability falls back to the auxValue read,
    // which is right for some items and wrong for others.
    s_getDamageValue = reinterpret_cast<DamageFn>(sigs.sig("ItemStackBase::getDamageValue"));
    if (!s_getDamageValue) {
        LOG_WARN("ItemStackBase::getDamageValue not resolved — durability values will be "
                 "approximated from auxValue and may be wrong for non-tool items");
    }

    m_resolved = true;
    return true;
}

void GameSDK::installHooks() {
    if (!m_resolved) return;

    auto& sigs  = SignatureManager::get();
    auto& hooks = HookManager::get();

    // Every hook here is optional: each drives exactly one feature, which
    // reports "--" or no-ops when its signature is missing.
    if (const auto addr = sigs.sig("Options::getGamma")) {
        hooks.create<GammaFn>("Options::getGamma",
                              reinterpret_cast<void*>(addr), &hkGetGamma, &o_getGamma);
    } else {
        LOG_WARN("Options::getGamma not resolved — Fullbright will no-op");
    }
    if (const auto addr = sigs.sig("Actor::attack")) {
        hooks.create<AttackFn>("Actor::attack",
                               reinterpret_cast<void*>(addr), &hkAttack, &o_attack);
    } else {
        LOG_WARN("Actor::attack not resolved — Reach display will show no data");
    }
    if (const auto addr = sigs.sig("RakPeer::GetAveragePing")) {
        hooks.create<PingFn>("RakPeer::GetAveragePing",
                             reinterpret_cast<void*>(addr), &hkGetAveragePing, &o_getAveragePing);
    } else {
        LOG_WARN("RakPeer::GetAveragePing not resolved — Ping display unavailable");
    }
    if (const auto addr = sigs.sig("Dimension::getTimeOfDay")) {
        hooks.create<TimeFn>("Dimension::getTimeOfDay",
                             reinterpret_cast<void*>(addr), &hkGetTimeOfDay, &o_getTimeOfDay);
    } else {
        LOG_WARN("Dimension::getTimeOfDay not resolved — Day Counter will show no data");
    }
}

// ─── Object graph ────────────────────────────────────────────────────────────

ClientInstance* GameSDK::clientInstance() const {
    if (!m_resolved || !m_gameCoreGlobal) return nullptr;

    // Cached, because Latite caches (`ClientInstance::get()` resolves once into
    // a static and returns it forever), and because walking it is not the cheap
    // three pointer reads the old comment here claimed: the last step is a
    // lock-free read of a std::map the game mutates on world load.
    //
    // Doing that on every frame AND every 10ms logic tick meant ~200 traversals
    // a second of the single most fragile structure Glacier touches, for a
    // value that changes about twice a session.
    //
    // Where this deviates from Latite: it re-walks periodically instead of
    // caching forever. Latite never invalidates, which leaves a dangling
    // ClientInstance after a disconnect; refreshing occasionally keeps the
    // 200x reduction while still noticing a world change.
    constexpr std::uint64_t kCacheMs = 500;

    const auto now = GetTickCount64();
    if (void* cached = m_cachedInstance.load(std::memory_order_relaxed)) {
        if (now - m_cacheStamp.load(std::memory_order_relaxed) < kCacheMs) {
            return static_cast<ClientInstance*>(cached);
        }
    }

    auto& sigs = SignatureManager::get();

    void* winMain = *reinterpret_cast<void**>(m_gameCoreGlobal);
    void* gameCore = deref(winMain, sigs.offset("WinMain::platformGameCore"));
    void* mcGame = deref(gameCore, sigs.offset("Platform_GameCore::minecraftGame"));
    if (!mcGame) return nullptr;

    // Entry 0 is the primary (local) instance. Guarded — see the comment on
    // readPrimaryInstance for why this specific read is the fragile one.
    GLACIER_ACTIVITY("reading the ClientInstance map");
    void* instance = readPrimaryInstance(mcGame, sigs.offset("MinecraftGame::clientInstances"));

    m_cachedInstance.store(instance, std::memory_order_relaxed);
    m_cacheStamp.store(now, std::memory_order_relaxed);

    LOG_ONCE("first ClientInstance resolved: {:#x}", reinterpret_cast<std::uintptr_t>(instance));
    return static_cast<ClientInstance*>(instance);
}

LocalPlayer* GameSDK::localPlayer() const {
    void* ci = clientInstance();
    if (!ci || m_localPlayerVIndex < 0) return nullptr;

    // getLocalPlayer's vtable index is a pinned literal (0x1F) carried from
    // upstream and never verified against this build. A wrong index does not
    // fault — it calls whatever function happens to sit in that slot, with the
    // wrong signature, which corrupts the stack and kills the process with no
    // catchable exception. That is indistinguishable from "it just crashed",
    // so the address is logged once and the call is bracketed: if the log ends
    // between these two lines, this is the bug.
    LOG_ONCE("first getLocalPlayer call: ClientInstance @ {:#x}, vtable[{}] = {:#x}",
             reinterpret_cast<std::uintptr_t>(ci), m_localPlayerVIndex,
             reinterpret_cast<std::uintptr_t>((*static_cast<void***>(ci))[m_localPlayerVIndex]));

    GLACIER_ACTIVITY("calling ClientInstance::getLocalPlayer");
    auto* player = memory::callVirtualI<LocalPlayer*>(
        static_cast<std::uint32_t>(m_localPlayerVIndex), ci);

    LOG_ONCE("first getLocalPlayer returned {:#x}",
             reinterpret_cast<std::uintptr_t>(player));
    return player;
}

std::optional<Vec3> GameSDK::playerPosition() const {
    auto* lp = localPlayer();
    if (!lp) return std::nullopt;

    auto& sigs = SignatureManager::get();

    // Position lives in an ECS component now; Actor caches a pointer to it.
    void* state = deref(lp, sigs.offset("Actor::stateVector"));
    if (!state) return std::nullopt;

    const Vec3 pos = memory::memberAt<Vec3>(state, sigs.offset("StateVectorComponent::pos"));
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
        return std::nullopt;
    }
    return pos;
}

void GameSDK::setGammaOverride(float gamma) {
    s_gammaOverride.store(gamma, std::memory_order_relaxed);
}

bool GameSDK::gammaHookActive() const {
    return o_getGamma != nullptr;
}

bool GameSDK::cursorControlAvailable() const {
    return s_grabCursor != nullptr && s_releaseCursor != nullptr;
}

bool GameSDK::cursorGrabbed() const {
    void* ci = clientInstance();
    if (!ci) return false;

    auto& sigs = SignatureManager::get();
    void* mcGame = deref(ci, sigs.offset("ClientInstance::minecraftGame"));
    if (!mcGame) return false;

    return memory::memberAt<bool>(mcGame, sigs.offset("MinecraftGame::cursorGrabbed"));
}

void GameSDK::applyCursorState(bool menuOpen) {
    // Once we've SEEN the call fail to do anything, stop trusting it and let
    // Glacier::setCursorReleased's ShowCursor/ClipCursor fallback take over
    // instead — better than calling a function every frame that's already
    // been proven not to work.
    if (!cursorControlAvailable() || !cursorControlWorking()) return;

    void* ci = clientInstance();
    if (!ci) {
        // No world. Don't remember a menu state across it, or re-entering one
        // would fire a stray grab on the first frame.
        m_menuWasOpen = false;
        return;
    }

    if (menuOpen) {
        // Re-assert every frame. See the header: one call does not hold.
        if (cursorGrabbed()) {
            GLACIER_ACTIVITY("calling ClientInstance::releaseCursor");
            s_releaseCursor(ci);

            // Confirms the call actually changed the game's own flag, not
            // just that it didn't crash. A signature can resolve to
            // something that isn't really releaseCursor for this build —
            // the same failure mode as the item-icon vptr bug — and still
            // return cleanly without ever touching cursorGrabbed.
            const bool stillGrabbed = cursorGrabbed();
            LOG_ONCE("releaseCursor called — cursorGrabbed() now reports {}",
                     stillGrabbed ? "still grabbed (did NOT take effect)" : "released (ok)");
            if (stillGrabbed) {
                LOG_WARN("ClientInstance::releaseCursor did not release the cursor — "
                         "falling back to ShowCursor for the rest of this session");
                m_cursorControlWorking.store(false, std::memory_order_relaxed);
            }
        }
    } else if (m_menuWasOpen) {
        GLACIER_ACTIVITY("calling ClientInstance::grabCursor");
        s_grabCursor(ci);
        LOG_ONCE("grabCursor called — cursorGrabbed() now reports {}",
                 cursorGrabbed() ? "grabbed (ok)" : "still released (did NOT take effect)");
    }
    m_menuWasOpen = menuOpen;
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

void GameSDK::recordAttack(void* attacker, void* target) {
    if (!attacker || !target) return;

    auto& sigs = SignatureManager::get();
    const auto stateOff = sigs.offset("Actor::stateVector");
    const auto posOff   = sigs.offset("StateVectorComponent::pos");

    void* attackerState = deref(attacker, stateOff);
    void* targetState   = deref(target, stateOff);
    if (!attackerState || !targetState) return;

    const Vec3 from = memory::memberAt<Vec3>(attackerState, posOff);
    const Vec3 to   = memory::memberAt<Vec3>(targetState, posOff);

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

int GameSDK::localPlayerInvulnerableTime() const {
    auto* lp = localPlayer();
    if (!lp) return -1;

    auto& sigs = SignatureManager::get();
    const auto off = sigs.offset("Actor::invulnerableTime");
    if (off == 0) return -1;

    return memory::memberAt<int>(lp, off);
}

std::optional<float> GameSDK::lastAttackDistance(std::uint64_t* ageMs) const {
    const auto stamp = m_attackStamp.load(std::memory_order_relaxed);
    if (stamp == 0) return std::nullopt;

    if (ageMs) *ageMs = GetTickCount64() - stamp;
    return m_attackDistance.load(std::memory_order_relaxed);
}

// ─── Containers ──────────────────────────────────────────────────────────────

ItemStack GameSDK::readStack(void* stackPtr) const {
    ItemStack out;
    if (!stackPtr) return out;

    auto& sigs = SignatureManager::get();

    // The field is an `Item**`. A null one means the slot is empty (air), which
    // is a valid answer rather than a failure.
    void* itemHandle = deref(stackPtr, sigs.offset("ItemStack::item"));
    if (!itemHandle) return out;

    out.valid = true;
    out.count = *reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(stackPtr) + sigs.offset("ItemStack::count"));

    out.damage = s_getDamageValue
        ? s_getDamageValue(stackPtr)
        : *reinterpret_cast<std::int16_t*>(
              reinterpret_cast<std::uintptr_t>(stackPtr) + sigs.offset("ItemStack::auxValue"));

    // One more hop to the Item itself, then ask it how much damage it takes to
    // break. 0 means "not damageable", which ItemStack already treats as "no
    // durability bar" — so nothing special is needed for blocks and food.
    void* item = *reinterpret_cast<void**>(itemHandle);
    const auto maxDamageIndex = sigs.offset("Item::getMaxDamageVIndex");
    if (item && maxDamageIndex > 0) {
        GLACIER_ACTIVITY("calling Item::getMaxDamage");
        out.maxDurability = memory::callVirtualI<int>(
            static_cast<std::uint32_t>(maxDamageIndex), item);
        // A bad vtable index returns whatever was in the register. Anything
        // outside the plausible range is discarded rather than rendered as a
        // confident, wrong durability bar.
        if (out.maxDurability < 0 || out.maxDurability > 10000) out.maxDurability = 0;
    }
    return out;
}

void* GameSDK::rawHotbarStack(int slot) const {
    auto* lp = localPlayer();
    if (!lp) return nullptr;

    auto& sigs = SignatureManager::get();

    void* supplies = deref(lp, sigs.offset("Player::supplies"));
    if (!supplies) return nullptr;

    void* inventory = deref(supplies, sigs.offset("PlayerInventory::inventory"));
    if (!inventory) return nullptr;

    if (slot < 0) {
        slot = memory::memberAt<int>(supplies, sigs.offset("PlayerInventory::selectedSlot"));
    }
    if (slot < 0 || slot > 8) return nullptr;   // hotbar only

    const auto index = static_cast<std::uint32_t>(sigs.offset("Inventory::getItemVIndex"));
    return memory::callVirtualI<void*, int>(index, inventory, slot);
}

std::array<ItemStack, 36> GameSDK::inventory() const {
    std::array<ItemStack, 36> out{};
    for (int slot = 0; slot < 36; ++slot) {
        out[static_cast<std::size_t>(slot)] = readStack(rawInventoryStack(slot));
    }
    return out;
}

void* GameSDK::rawInventoryStack(int slot) const {
    if (slot < 0 || slot > 35) return nullptr;

    auto* lp = localPlayer();
    if (!lp) return nullptr;

    auto& sigs = SignatureManager::get();
    void* supplies = deref(lp, sigs.offset("Player::supplies"));
    if (!supplies) return nullptr;

    void* container = deref(supplies, sigs.offset("PlayerInventory::inventory"));
    if (!container) return nullptr;

    const auto index = static_cast<std::uint32_t>(sigs.offset("Inventory::getItemVIndex"));
    return memory::callVirtualI<void*, int>(index, container, slot);
}

void* GameSDK::rawArmorStack(int slot) const {
    if (slot < 0 || slot > 3) return nullptr;

    auto* lp = localPlayer();
    if (!lp) return nullptr;

    auto& sigs = SignatureManager::get();

    const auto ctxOff = sigs.offset("Actor::entityContext");
    if (ctxOff == 0) return nullptr;

    const auto& ctx = memory::memberAt<EntityContext>(lp, ctxOff);
    if (!ctx.enttRegistry) return nullptr;

    const auto* equipment = ctx.enttRegistry->try_get<ActorEquipmentComponent>(ctx.entity);
    if (!equipment || !equipment->armorContainer) return nullptr;

    const auto index = static_cast<std::uint32_t>(sigs.offset("Inventory::getItemVIndex"));
    return memory::callVirtualI<void*, int>(index, equipment->armorContainer, slot);
}

float GameSDK::guiScaleFrac() const {
    void* ci = clientInstance();
    if (!ci) return 0.0f;

    auto& sigs = SignatureManager::get();

    void* guiData = deref(ci, sigs.offset("ClientInstance::guiData"));
    if (!guiData) return 0.0f;

    const float frac = memory::memberAt<float>(guiData, sigs.offset("GuiData::guiScaleFrac"));
    // GUI scale is 1..4-ish, so the reciprocal lives in (0.25, 1]. Anything
    // else means the offset is wrong, and drawing with it would put icons
    // somewhere off-screen instead of visibly failing.
    if (!std::isfinite(frac) || frac <= 0.01f || frac > 1.0f) return 0.0f;
    return frac;
}

// Inventory::getItem is virtual, and both of these go through it rather than
// walking the backing vector by hand: the game does its own bounds checking,
// and one vtable index is a far smaller thing to keep correct than a stride
// plus two container pointers. See rawHotbarStack / rawArmorStack.
ItemStack GameSDK::heldItem() const {
    return readStack(rawHotbarStack(-1));
}

std::array<ItemStack, 4> GameSDK::armor() const {
    // Armor lives in an ECS component, so each slot is reached through the
    // game's own entt registry rather than a fixed Actor offset. See
    // EntityComponents.h for why this is the most layout-sensitive read in the
    // SDK. The registry walk repeats per slot; at four slots once a frame that
    // is not worth the coupling of a batched variant.
    std::array<ItemStack, 4> out{};
    for (int slot = 0; slot < 4; ++slot) {
        out[static_cast<std::size_t>(slot)] = readStack(rawArmorStack(slot));
    }
    return out;
}

bool GameSDK::armorSupported() const {
    // The component lookup needs the entity context offset; without it there is
    // no route to the registry at all.
    return SignatureManager::get().offset("Actor::entityContext") != 0;
}

std::array<ItemStack, 9> GameSDK::hotbar() const {
    std::array<ItemStack, 9> out{};
    for (int slot = 0; slot < 9; ++slot) {
        out[static_cast<std::size_t>(slot)] = readStack(rawHotbarStack(slot));
    }
    return out;
}

int GameSDK::selectedHotbarSlot() const {
    auto* lp = localPlayer();
    if (!lp) return -1;

    auto& sigs = SignatureManager::get();
    void* supplies = deref(lp, sigs.offset("Player::supplies"));
    if (!supplies) return -1;

    const int slot = memory::memberAt<int>(supplies, sigs.offset("PlayerInventory::selectedSlot"));
    return (slot >= 0 && slot <= 8) ? slot : -1;
}

} // namespace glacier::sdk
