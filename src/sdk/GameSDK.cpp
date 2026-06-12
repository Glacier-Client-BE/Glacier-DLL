#include "GameSDK.h"

#include "../hook/HookManager.h"
#include "../memory/SignatureManager.h"
#include "../util/Logger.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <unordered_set>
#include <Windows.h>

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
    GameSDK::get().applyQueuedTurn();   // Snap Look turns run on the game thread
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

// ── GameMode::attack hook (>= 1.21.50 shape) ──
// Fires on the game thread for every melee attack the client simulates. The SDK
// records who-hit-whom + distance for the reach/ping HUD modules.
using AttackFn = void(__fastcall*)(void*, void*, bool);
AttackFn o_attack = nullptr;

void __fastcall hkAttack(void* gameMode, void* target, bool a3) {
    GameSDK::get().recordAttack(gameMode, target);
    o_attack(gameMode, target, a3);
}

// ── RakPeer::GetAveragePing hook ──
// The game queries the peer's RTT itself; we cache whatever it sees.
using PingFn = std::int64_t(__fastcall*)(void*, void*);
PingFn o_getAveragePing = nullptr;

std::int64_t __fastcall hkGetAveragePing(void* peer, void* addressOrGuid) {
    const std::int64_t ping = o_getAveragePing(peer, addressOrGuid);
    if (ping >= 0 && ping < 10'000) {
        GameSDK::get().cachePing(static_cast<int>(ping));
    }
    return ping;
}

// ── BobHurt hook (camera/hand bob matrix) ──
// Java View Bobbing injects a translation into the bob matrix; (0,0) is a pure
// pass-through. glm::mat4 is column-major: column 3 holds the translation.
using BobFn = void(__fastcall*)(void*, float*);
BobFn o_bobHurt = nullptr;
std::atomic<float> s_bobX{ 0.0f };
std::atomic<float> s_bobY{ 0.0f };

void __fastcall hkBobHurt(void* self, float* m) {
    const float x = s_bobX.load(std::memory_order_relaxed);
    const float y = s_bobY.load(std::memory_order_relaxed);
    if (m && (x != 0.0f || y != 0.0f)) {
        // m = translate(m, {x, y, 0}) — column3 += column0*x + column1*y.
        for (int r = 0; r < 4; ++r) {
            m[12 + r] += m[r] * x + m[4 + r] * y;
        }
    }
    o_bobHurt(self, m);
}

// ── TimeChanger hook (time-of-day "% 24000" helper) ──
// Read-only: the target begins `mov eax, edx; … imul edx`, i.e. it takes the
// raw tick count in the 2nd integer arg and returns an int — so the detour must
// mirror that ABI exactly (an int passthrough never corrupts the result, unlike
// a float return reading xmm0). We snapshot the input ticks for the Day Counter.
using TimeFn = int(__fastcall*)(void*, std::int64_t);
TimeFn o_timeOfDay = nullptr;

int __fastcall hkTimeOfDay(void* a1, std::int64_t rawTicks) {
    const auto ticks = static_cast<int>(rawTicks);
    if (ticks >= 0 && ticks < 1'000'000'000) {
        GameSDK::get().cacheWorldTime(ticks);
    }
    return o_timeOfDay(a1, rawTicks);
}

// ── LocalPlayer::applyTurnDelta hook ──
// Pure pass-through detour; the value is the trampoline, which lets the SDK
// apply its own queued {pitch, yaw} turns from the game thread (Snap Look).
using TurnFn = void(__fastcall*)(void*, float*);
TurnFn o_applyTurnDelta = nullptr;

void __fastcall hkApplyTurnDelta(void* localPlayer, float* delta) {
    o_applyTurnDelta(localPlayer, delta);
}

std::atomic<float> s_pendingYaw{ 0.0f };
std::atomic<float> s_pendingPitch{ 0.0f };

// ── AppPlatform::readAssetFile hook (Material Bin Loader) ──
// Core::Path is a thin std::string wrapper, so the path argument can be read as
// a string directly. While the redirect is on, *.material.bin requests are
// answered from `materials/` beside the DLL when a replacement exists.
using ReadFileFn = std::string*(__fastcall*)(void*, std::string*, void*);
ReadFileFn o_readAssetFile = nullptr;
std::atomic<bool> s_materialRedirect{ false };

std::string* __fastcall hkReadAssetFile(void* self, std::string* ret, void* path) {
    std::string* result = o_readAssetFile(self, ret, path);
    if (!s_materialRedirect.load(std::memory_order_relaxed) || !path || !result) {
        return result;
    }

    const std::string& p = *reinterpret_cast<const std::string*>(path);
    constexpr std::string_view kExt = ".material.bin";
    if (p.size() <= kExt.size() || p.compare(p.size() - kExt.size(), kExt.size(), kExt) != 0) {
        return result;
    }

    const auto slash = p.find_last_of("/\\");
    const std::string fileName = (slash == std::string::npos) ? p : p.substr(slash + 1);

    // Weather.material.bin replacements are a known crash source (same skip as
    // BetterRenderDragon / Flarial).
    if (fileName == "Weather.material.bin") return result;

    const auto replacement = std::filesystem::path(GameSDK::materialsDir()) / fileName;
    std::error_code ec;
    if (!std::filesystem::exists(replacement, ec)) return result;

    std::ifstream in(replacement, std::ios::binary);
    if (!in) return result;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.empty()) return result;

    result->assign(std::move(data));

    static std::unordered_set<std::string> logged;
    if (logged.insert(fileName).second) {
        LOG_INFO("MaterialBinLoader: substituted {}", fileName);
    }
    return result;
}

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

    // Optional instrumentation hooks. Each module that depends on one degrades
    // gracefully (HUD shows "—" / feature no-ops) when its signature is missing,
    // so none of these are attach-blocking.
    if (const auto addr = sigs.sig("GameMode::attack")) {
        hooks.create<AttackFn>("GameMode::attack",
                               reinterpret_cast<void*>(addr), &hkAttack, &o_attack);
    } else {
        LOG_WARN("GameMode::attack not resolved — reach/hit modules show no data");
    }
    if (const auto addr = sigs.sig("RakPeer::GetAveragePing")) {
        hooks.create<PingFn>("RakPeer::GetAveragePing",
                             reinterpret_cast<void*>(addr), &hkGetAveragePing, &o_getAveragePing);
    } else {
        LOG_WARN("RakPeer::GetAveragePing not resolved — ping unavailable");
    }
    if (const auto addr = sigs.sig("BobHurt")) {
        hooks.create<BobFn>("BobHurt",
                            reinterpret_cast<void*>(addr), &hkBobHurt, &o_bobHurt);
    } else {
        LOG_WARN("BobHurt not resolved — Java View Bobbing will no-op");
    }
    if (const auto addr = sigs.sig("TimeChanger")) {
        hooks.create<TimeFn>("TimeChanger",
                             reinterpret_cast<void*>(addr), &hkTimeOfDay, &o_timeOfDay);
    } else {
        LOG_WARN("TimeChanger not resolved — Day Counter shows no data");
    }
    if (const auto addr = sigs.sig("LocalPlayer::applyTurnDelta")) {
        hooks.create<TurnFn>("LocalPlayer::applyTurnDelta",
                             reinterpret_cast<void*>(addr), &hkApplyTurnDelta, &o_applyTurnDelta);
    } else {
        LOG_WARN("LocalPlayer::applyTurnDelta not resolved — Snap Look will no-op");
    }
    if (const auto addr = sigs.sig("AppPlatform::readAssetFile")) {
        hooks.create<ReadFileFn>("AppPlatform::readAssetFile",
                                 reinterpret_cast<void*>(addr), &hkReadAssetFile, &o_readAssetFile);
    } else {
        LOG_WARN("AppPlatform::readAssetFile not resolved — Material Bin Loader will no-op");
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

// ─── Attack instrumentation ───────────────────────────────────────────────────

void GameSDK::recordAttack(void* gameMode, void* target) {
    if (!gameMode || !target) return;

    auto& sigs = SignatureManager::get();
    void* attacker = memory::memberAt<void*>(gameMode, sigs.offset("Gamemode::player"));
    void* lp = localPlayer();
    if (!attacker || !lp) return;

    const bool outgoing = (attacker == lp);
    const bool incoming = (!outgoing && target == lp);
    if (!outgoing && !incoming) return;

    // Distance between attacker and target (feet positions). A closest-point-
    // on-AABB refinement needs the hitbox component; this approximation is
    // within ~0.3 blocks for player-sized targets.
    const auto posOff = sigs.offset("Actor::position");
    const Vec3 a = memory::memberAt<Vec3>(outgoing ? lp : attacker, posOff);
    const Vec3 b = memory::memberAt<Vec3>(target, posOff);
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;

    AttackInfo info;
    info.targetPos = b;
    info.distance  = std::sqrt(dx * dx + dy * dy + dz * dz);
    info.timeMs    = GetTickCount64();
    info.valid     = true;

    std::scoped_lock lock(m_attackMutex);
    info.seq = ++m_attackSeq;
    (outgoing ? m_lastAttack : m_lastIncoming) = info;
}

AttackInfo GameSDK::lastAttack() const {
    std::scoped_lock lock(m_attackMutex);
    return m_lastAttack;
}

AttackInfo GameSDK::lastIncomingHit() const {
    std::scoped_lock lock(m_attackMutex);
    return m_lastIncoming;
}

// ─── Ping / world-time caches ─────────────────────────────────────────────────

void GameSDK::cachePing(int ms) {
    m_ping.store(ms, std::memory_order_relaxed);
    m_pingStamp.store(GetTickCount64(), std::memory_order_relaxed);
}

int GameSDK::ping() const {
    // Stale after 10 s (left the server / the game stopped querying).
    if (GetTickCount64() - m_pingStamp.load(std::memory_order_relaxed) > 10'000) return -1;
    return m_ping.load(std::memory_order_relaxed);
}

void GameSDK::cacheWorldTime(int ticks) {
    m_worldTime.store(ticks, std::memory_order_relaxed);
    m_worldTimeStamp.store(GetTickCount64(), std::memory_order_relaxed);
}

std::optional<int> GameSDK::worldTime() const {
    if (GetTickCount64() - m_worldTimeStamp.load(std::memory_order_relaxed) > 5'000) {
        return std::nullopt;
    }
    const int t = m_worldTime.load(std::memory_order_relaxed);
    return t >= 0 ? std::optional<int>(t) : std::nullopt;
}

// ─── View bobbing / camera turns ──────────────────────────────────────────────

void GameSDK::setBobTranslation(float x, float y) {
    s_bobX.store(x, std::memory_order_relaxed);
    s_bobY.store(y, std::memory_order_relaxed);
}

void GameSDK::queueTurn(float yawDeg, float pitchDeg) {
    // Accumulate so rapid taps between ticks aren't lost.
    float expected = s_pendingYaw.load(std::memory_order_relaxed);
    while (!s_pendingYaw.compare_exchange_weak(expected, expected + yawDeg)) {}
    expected = s_pendingPitch.load(std::memory_order_relaxed);
    while (!s_pendingPitch.compare_exchange_weak(expected, expected + pitchDeg)) {}
}

void GameSDK::applyQueuedTurn() {
    if (!o_applyTurnDelta) return;

    const float yaw   = s_pendingYaw.exchange(0.0f, std::memory_order_relaxed);
    const float pitch = s_pendingPitch.exchange(0.0f, std::memory_order_relaxed);
    if (yaw == 0.0f && pitch == 0.0f) return;

    if (void* lp = localPlayer()) {
        float delta[2] = { pitch, yaw };   // Vec2 {x = pitch, y = yaw}
        o_applyTurnDelta(lp, delta);
    }
}

bool GameSDK::canTurn() const {
    return o_applyTurnDelta != nullptr;
}

// ─── Entity hitbox projection ─────────────────────────────────────────────────

namespace {

// Minimal column-major 4x4 (glm-compatible memory layout): m[col*4 + row].
struct M4 { float m[16]; };

// clip = mat * (x,y,z,w), column-major.
struct V4 { float x, y, z, w; };
V4 mul(const M4& a, const V4& v) {
    V4 r;
    r.x = a.m[0]*v.x + a.m[4]*v.y + a.m[8]*v.z  + a.m[12]*v.w;
    r.y = a.m[1]*v.x + a.m[5]*v.y + a.m[9]*v.z  + a.m[13]*v.w;
    r.z = a.m[2]*v.x + a.m[6]*v.y + a.m[10]*v.z + a.m[14]*v.w;
    r.w = a.m[3]*v.x + a.m[7]*v.y + a.m[11]*v.z + a.m[15]*v.w;
    return r;
}
// c = a * b, column-major.
M4 mul(const M4& a, const M4& b) {
    M4 c{};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[k*4 + row] * b.m[col*4 + k];
            c.m[col*4 + row] = s;
        }
    return c;
}

// Stub type whose member-function pointer matches Level::getRuntimeActorList,
// so the engine call gets MSVC's correct sret-+-this ABI (Flarial's technique).
struct LevelStub {
    std::vector<void*> getRuntimeActorList();
};

bool finite3(const Vec3& v) {
    auto ok = [](float f) { return f > -3.0e7f && f < 3.0e7f; };
    return ok(v.x) && ok(v.y) && ok(v.z);
}

} // namespace

std::optional<std::pair<float, float>> GameSDK::worldToScreen(const Vec3& world) const {
    void* ci = m_clientInstance.load(std::memory_order_relaxed);
    if (!ci) return std::nullopt;

    auto& sigs = SignatureManager::get();

    // MinecraftGame -> GameRenderer -> the two per-frame matrices.
    void* mcGame = memory::memberAt<void*>(ci, sigs.offset("ClientInstance::minecraftGame"));
    if (!mcGame) return std::nullopt;
    void* gameRenderer = memory::memberAt<void*>(mcGame, sigs.offset("MinecraftGame::gameRenderer"));
    if (!gameRenderer) return std::nullopt;

    const M4& view = memory::memberAt<M4>(gameRenderer, sigs.offset("GameRenderer::viewMatrix"));
    const M4& proj = memory::memberAt<M4>(gameRenderer, sigs.offset("GameRenderer::projMatrix"));

    // GuiData -> render-target pixel size.
    void* guiData = memory::memberAt<void*>(ci, sigs.offset("ClientInstance::guiData"));
    if (!guiData) return std::nullopt;
    const auto screenW = memory::memberAt<float>(guiData, sigs.offset("GuiData::screenSize"));
    const auto screenH = memory::memberAt<float>(guiData, sigs.offset("GuiData::screenSize") + sizeof(float));
    if (screenW < 1.0f || screenH < 1.0f) return std::nullopt;

    // Camera render origin (LevelRenderer player cameraPos), fallback to player.
    Vec3 origin = cameraOrigin();

    const Vec3 rel{ world.x - origin.x, world.y - origin.y, world.z - origin.z };
    const M4 mvp = mul(proj, view);
    const V4 clip = mul(mvp, V4{ rel.x, rel.y, rel.z, 1.0f });
    if (clip.w <= 0.01f) return std::nullopt;

    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    if (ndcX < -1.05f || ndcX > 1.05f || ndcY < -1.05f || ndcY > 1.05f) return std::nullopt;

    const float sx = (ndcX + 1.0f) * 0.5f * screenW;
    const float sy = (1.0f - ndcY) * 0.5f * screenH;
    return std::make_pair(sx, sy);
}

Vec3 GameSDK::cameraOrigin() const {
    void* ci = m_clientInstance.load(std::memory_order_relaxed);
    auto& sigs = SignatureManager::get();

    // LevelRenderer is a virtual on 1.21.120+ — call the resolved vtable index.
    if (ci) {
        const auto idx = sigs.offset("ClientInstance::getLevelRendererVIndex");
        if (idx > 0) {
            void* levelRenderer =
                memory::callVirtualI<void*>(static_cast<std::uint32_t>(idx), ci);
            if (levelRenderer) {
                void* lrp = memory::memberAt<void*>(
                    levelRenderer, sigs.offset("LevelRenderer::levelRendererPlayer"));
                if (lrp) {
                    const Vec3 cam = memory::memberAt<Vec3>(
                        lrp, sigs.offset("LevelRendererPlayer::cameraPos"));
                    if (finite3(cam)) return cam;
                }
            }
        }
    }
    // Fallback: the local player's feet position.
    if (auto p = playerPosition()) return *p;
    return Vec3{};
}

std::vector<ScreenBox> GameSDK::projectHitboxes(float maxDistance, float lookLineLen) const {
    std::vector<ScreenBox> out;

    auto& sigs = SignatureManager::get();
    const auto listSig = sigs.sig("Level::getRuntimeActorList");
    if (listSig == 0) return out;            // no engine enumerator → nothing

    void* self = localPlayer();
    if (!self) return out;

    void* level = memory::memberAt<void*>(self, sigs.offset("Actor::level"));
    if (!level) return out;

    // Call the engine's thread-safe enumerator via a member-function pointer so
    // the std::vector return ABI is correct.
    using RalPtr = std::vector<void*>(LevelStub::*)();
    RalPtr fn = *reinterpret_cast<RalPtr*>(const_cast<std::uintptr_t*>(&listSig));
    std::vector<void*> actors = (reinterpret_cast<LevelStub*>(level)->*fn)();

    const auto posOff = sigs.offset("Actor::position");
    const Vec3 selfPos = memory::memberAt<Vec3>(self, posOff);

    out.reserve(actors.size());
    for (void* actor : actors) {
        if (!actor || actor == self) continue;

        const Vec3 pos = memory::memberAt<Vec3>(actor, posOff);
        if (!finite3(pos)) continue;

        const float dx = pos.x - selfPos.x, dy = pos.y - selfPos.y, dz = pos.z - selfPos.z;
        if (dx*dx + dy*dy + dz*dz > maxDistance * maxDistance) continue;

        // Standard humanoid AABB centred on the actor's feet position. (Exact
        // per-entity dimensions need the AABBShapeComponent; this matches the
        // common player/mob case and avoids the fragile component lookup.)
        constexpr float kHalf = 0.30f, kHeight = 1.80f;
        const Vec3 lo{ pos.x - kHalf, pos.y,           pos.z - kHalf };
        const Vec3 hi{ pos.x + kHalf, pos.y + kHeight, pos.z + kHalf };

        ScreenBox box;
        // Corner order must match the web edge table (BOX_EDGES in main.js):
        //   0:lo  1:(hi.x,lo.y,lo.z) 2:(hi.x,lo.y,hi.z) 3:(lo.x,lo.y,hi.z)
        //   4:(lo.x,hi.y,lo.z) 5:(hi.x,hi.y,lo.z) 6:hi 7:(lo.x,hi.y,hi.z)
        const Vec3 c[8] = {
            { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { lo.x, lo.y, hi.z },
            { lo.x, hi.y, lo.z }, { hi.x, hi.y, lo.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z },
        };
        int visible = 0;
        for (int i = 0; i < 8; ++i) {
            if (auto s = worldToScreen(c[i])) {
                box.corners[i] = { s->first, s->second, true };
                ++visible;
            }
        }
        if (visible == 0) continue;          // entirely off-screen / behind

        (void)lookLineLen;                   // per-entity facing needs rotation
        out.push_back(box);                  // component; omitted for now.

        if (out.size() >= 64) break;         // hard cap — perf guard
    }
    return out;
}

// ─── Material redirect ────────────────────────────────────────────────────────

void GameSDK::setMaterialRedirect(bool enabled) {
    s_materialRedirect.store(enabled, std::memory_order_relaxed);
    if (enabled) {
        std::error_code ec;
        std::filesystem::create_directories(materialsDir(), ec);
    }
}

std::string GameSDK::materialsDir() {
    char path[MAX_PATH]{};
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&GameSDK::materialsDir), &self);
    GetModuleFileNameA(self, path, MAX_PATH);
    return (std::filesystem::path(path).parent_path() / "materials").string();
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
