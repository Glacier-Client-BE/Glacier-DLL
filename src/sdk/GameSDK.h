#pragma once

#include <array>
#include <atomic>
#include <cstdint>
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

private:
    GameSDK() = default;

    ItemStack readSlot(std::uintptr_t containerBase, int index) const;

    bool m_resolved = false;

    // Captured live by the update hook.
    std::atomic<void*> m_clientInstance{ nullptr };

    // Decoded once in resolve().
    int m_localPlayerVIndex = -1;
};

} // namespace glacier::sdk
