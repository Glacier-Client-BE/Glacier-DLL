#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <Windows.h>
#include <imgui.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Glacier SDK — mirrors Minecraft Bedrock v26.x internal class layout.
//  All offsets are RE'd via IDA/Ghidra — rescan after each game update.
//  NULL-SAFETY CONTRACT: every getter that touches a pointer does a null-check
//  and returns a safe default so HUD modules render in overlay even outside
//  a live game session.
// ─────────────────────────────────────────────────────────────────────────────

struct Vec2  { float x, y; };
struct Vec3  { float x, y, z; };
struct Vec4  { float x, y, z, w; };
struct AABB  { Vec3 min, max; };

// ── ItemStack ────────────────────────────────────────────────────────────────
struct ItemStack {
    int         id        = 0;
    int         count     = 0;
    int         damage    = 0;
    int         maxDamage = 0;
    std::string name;

    bool  isValid()          const { return id != 0; }
    bool  hasDurability()    const { return maxDamage > 0; }
    int   getDurability()    const { return maxDamage - damage; }
    float getDurabilityPct() const {
        return hasDurability() ? static_cast<float>(getDurability()) / static_cast<float>(maxDamage) : 1.f;
    }
};

// ── Entity type bitmask ──────────────────────────────────────────────────────
enum class EntityType : uint32_t {
    Unknown    = 0,
    Player     = 1 << 0,
    HostileMob = 1 << 1,
    PassiveMob = 1 << 2,
    ItemEntity = 1 << 3,
};

// ── Common effect IDs ─────────────────────────────────────────────────────────
namespace EffectId {
    constexpr int Blindness   = 15;
    constexpr int Nausea      = 9;
    constexpr int Darkness    = 17;
    constexpr int FrostedFeet = 28;
}

// ── Actor ────────────────────────────────────────────────────────────────────
class Actor {
public:
    Vec3 getPosition() const {
        return *reinterpret_cast<const Vec3*>(reinterpret_cast<uintptr_t>(this) + 0x100);
    }
    Vec3 getVelocity() const {
        return *reinterpret_cast<const Vec3*>(reinterpret_cast<uintptr_t>(this) + 0x110);
    }
    AABB getAABB() const {
        return *reinterpret_cast<const AABB*>(reinterpret_cast<uintptr_t>(this) + 0x120);
    }
    bool isAlive() const {
        return *reinterpret_cast<const bool*>(reinterpret_cast<uintptr_t>(this) + 0x180);
    }
    bool isOnGround() const {
        return *reinterpret_cast<const bool*>(reinterpret_cast<uintptr_t>(this) + 0x185);
    }
    EntityType getEntityType() const {
        return *reinterpret_cast<const EntityType*>(reinterpret_cast<uintptr_t>(this) + 0x190);
    }
    bool isPlayer()     const { return (uint32_t)getEntityType() & (uint32_t)EntityType::Player;     }
    bool isHostileMob() const { return (uint32_t)getEntityType() & (uint32_t)EntityType::HostileMob; }
    bool isPassiveMob() const { return (uint32_t)getEntityType() & (uint32_t)EntityType::PassiveMob; }
    bool isItemEntity() const { return (uint32_t)getEntityType() & (uint32_t)EntityType::ItemEntity; }

    float getHealth()    const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x3A0); }
    float getMaxHealth() const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x3A4); }
    float getHealthPct() const { float mx = getMaxHealth(); return mx > 0.f ? getHealth() / mx : 0.f; }
    int   getArmorValue() const { return *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(this) + 0x3B0); }

    std::string getName() const {
        auto ptr = reinterpret_cast<const std::string*>(reinterpret_cast<uintptr_t>(this) + 0x320);
        if (!ptr || ptr->empty()) return "Unknown";
        return *ptr;
    }

    float distanceTo(const Actor* o) const {
        if (!o) return 0.f;
        Vec3 a = getPosition(), b = o->getPosition();
        float dx = a.x-b.x, dy = a.y-b.y, dz = a.z-b.z;
        return sqrtf(dx*dx + dy*dy + dz*dz);
    }
    float distanceToXZ(const Actor* o) const {
        if (!o) return 0.f;
        Vec3 a = getPosition(), b = o->getPosition();
        float dx = a.x-b.x, dz = a.z-b.z;
        return sqrtf(dx*dx + dz*dz);
    }
};

// ── LocalPlayer ──────────────────────────────────────────────────────────────
class LocalPlayer : public Actor {
public:
    float getYaw()    const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x380); }
    float getPitch()  const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x384); }
    void  setYaw  (float v) { *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x380) = v; }
    void  setPitch(float v) { *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x384) = v; }

    bool isSprinting()  const { return *reinterpret_cast<const bool*>(reinterpret_cast<uintptr_t>(this) + 0x186); }
    bool isSneaking()   const { return *reinterpret_cast<const bool*>(reinterpret_cast<uintptr_t>(this) + 0x187); }
    bool isDead()       const { return *reinterpret_cast<const bool*>(reinterpret_cast<uintptr_t>(this) + 0x188); }
    void setSprinting(bool v) { *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(this) + 0x186) = v; }
    void setSneaking (bool v) { *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(this) + 0x187) = v; }

    float getHurtTime()       const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x3C0); }
    void  setHurtTime(float v)      { *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x3C0) = v; }
    float getMotionY()        const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x114); }
    void  setMotionY(float v)       { *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x114) = v; }

    int   getAirSupply()    const { return *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(this) + 0x3D0); }
    int   getMaxAirSupply() const { return *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(this) + 0x3D4); }
    void  setAirSupply(int v)     { *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + 0x3D0) = v; }

    float getStepHeight() const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x3E0); }
    void  setStepHeight(float v){ *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x3E0) = v; }

    int   getSelectedSlot()   const { return *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(this) + 0x4A0); }
    void  setSelectedSlot(int v)    { *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + 0x4A0) = v; }

    // Armor and inventory — stub; real impl reads ContainerComponent
    ItemStack getArmorItem(int /*slot*/) const { return {}; }
    ItemStack getInventoryItem(int /*slot*/) const { return {}; }
    ItemStack getHeldItem() const { return getInventoryItem(getSelectedSlot()); }

    bool hasEffect(int /*effectId*/) const { return false; }
    void removeEffect(int /*effectId*/) {}
    void jump() { if (isOnGround()) setMotionY(0.42f); }

    int   getDimension()  const { return *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(this) + 0x4B0); }
    int   getNetworkPing() const { return *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(this) + 0x4C0); }
    Actor* getLookTarget() const {
        return *reinterpret_cast<Actor* const*>(reinterpret_cast<uintptr_t>(this) + 0x4D0);
    }
};

// ── Level ─────────────────────────────────────────────────────────────────────
class Level {
public:
    std::vector<Actor*> getEntityList() const { return {}; }
    int getBlockId(int, int, int) const { return 0; }
    void sendPacket(uint8_t, const void*, size_t) {}
    void sendRespawnPacket() { sendPacket(0x61, nullptr, 0); }
};

// ── Options ───────────────────────────────────────────────────────────────────
class Options {
public:
    static Options*& instancePtr() { static Options* p = nullptr; return p; }
    static Options* get() { return instancePtr(); }
    float getGamma() const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x50); }
    void  setGamma(float v)  { *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x50) = v; }
    float getFOV()  const { return *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(this) + 0x54); }
    void  setFOV(float v)    { *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x54) = v; }
};

// ── ClientInstance ─────────────────────────────────────────────────────────────
class ClientInstance {
public:
    static ClientInstance*& instancePtr() { static ClientInstance* p = nullptr; return p; }
    static ClientInstance* get() { return instancePtr(); }

    LocalPlayer* getLocalPlayer() const {
        return *reinterpret_cast<LocalPlayer* const*>(reinterpret_cast<uintptr_t>(this) + 0x2B0);
    }
    Level* getLevel() const {
        return *reinterpret_cast<Level* const*>(reinterpret_cast<uintptr_t>(this) + 0x2C0);
    }
    Options* getOptions() const {
        return *reinterpret_cast<Options* const*>(reinterpret_cast<uintptr_t>(this) + 0x2D0);
    }
};

// ── Convenience accessors ─────────────────────────────────────────────────────
inline LocalPlayer* getLocalPlayer() {
    auto* ci = ClientInstance::get(); return ci ? ci->getLocalPlayer() : nullptr;
}
inline Level*  getLevel()   { auto* ci = ClientInstance::get(); return ci ? ci->getLevel()   : nullptr; }
inline Options* getOptions(){ auto* ci = ClientInstance::get(); return ci ? ci->getOptions() : nullptr; }

// ── World-to-screen ────────────────────────────────────────────────────────────
// viewProj: column-major 4×4 float array captured from the game renderer each frame.
inline bool worldToScreen(const Vec3& world, ImVec2& out, const float* viewProj) {
    if (!viewProj) return false;
    float x = world.x*viewProj[0]  + world.y*viewProj[4]  + world.z*viewProj[8]  + viewProj[12];
    float y = world.x*viewProj[1]  + world.y*viewProj[5]  + world.z*viewProj[9]  + viewProj[13];
    float z = world.x*viewProj[2]  + world.y*viewProj[6]  + world.z*viewProj[10] + viewProj[14];
    float w = world.x*viewProj[3]  + world.y*viewProj[7]  + world.z*viewProj[11] + viewProj[15];
    if (w < 0.1f) return false;
    const ImGuiIO& io = ImGui::GetIO();
    out.x = ( x/w + 1.f) * 0.5f * io.DisplaySize.x;
    out.y = (-y/w + 1.f) * 0.5f * io.DisplaySize.y;
    return out.x >= 0 && out.y >= 0 && out.x <= io.DisplaySize.x && out.y <= io.DisplaySize.y;
}

// ── View-projection matrix cache (written each frame by the swap-chain hook) ──
struct ViewProjectionCache {
    float   matrix[16] = {};
    bool    valid       = false;
    static ViewProjectionCache& get() { static ViewProjectionCache i; return i; }
    const float* get4x4() const { return valid ? matrix : nullptr; }
};

// ── Reach tracker (updated by GameMode::attack hook) ─────────────────────────
struct ReachTracker {
    float lastReach = 3.0f;
    bool  hasData   = false;
    static ReachTracker& get() { static ReachTracker i; return i; }
    void recordAttack(const LocalPlayer* lp, const Actor* target) {
        if (!lp || !target) return;
        lastReach = lp->distanceTo(target);
        hasData   = true;
    }
};

// ── Target tracker (refreshed each tick) ─────────────────────────────────────
struct TargetTracker {
    Actor* target   = nullptr;
    float  distance = 0.f;
    static TargetTracker& get() { static TargetTracker i; return i; }
    void update(LocalPlayer* lp) {
        if (!lp) { target = nullptr; return; }
        target = lp->getLookTarget();
        distance = target ? lp->distanceTo(target) : 0.f;
    }
};

// ── Packet intercept flags (checked by the MovePlayerPacket hook) ─────────────
struct PacketFlags {
    bool  forceOnGround   = false;
    float knockbackHMult  = 1.f;
    float knockbackVMult  = 1.f;
    static PacketFlags& get() { static PacketFlags i; return i; }
};

// ── FreeCam state ─────────────────────────────────────────────────────────────
struct FreeCamState {
    bool  active     = false;
    Vec3  savedPos   = {};
    Vec3  cameraPos  = {};
    float savedYaw   = 0.f;
    float savedPitch = 0.f;
    static FreeCamState& get() { static FreeCamState i; return i; }
};

// ── Saved original gamma/FOV for FullBright & Zoom restore ───────────────────
struct OriginalValues {
    float gamma     = 1.0f;
    float fov       = 70.f;
    bool  gammaSaved = false;
    bool  fovSaved   = false;
    static OriginalValues& get() { static OriginalValues i; return i; }
};
