#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

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

private:
    GameSDK() = default;

    bool m_resolved = false;

    // Captured live by the update hook, read from the logic + render threads.
    std::atomic<void*> m_clientInstance{ nullptr };

    // Decoded once in resolve() from the getLocalPlayerIndex call site.
    int m_localPlayerVIndex = -1;
};

} // namespace glacier::sdk
