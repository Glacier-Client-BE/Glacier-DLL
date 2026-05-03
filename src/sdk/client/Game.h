#pragma once
//
// Single point of access for "current game state". Every module talks to the
// game through this class, never by dereferencing raw pointers, so when the
// real SDK is wired up later the module code does not change.
//
// Until Addresses.h is filled in with real offsets, the getters return
// safe defaults (nullopt / empty / nullptr) and modules guard accordingly.
//
#include "GameTypes.h"
#include "../math/Vec3.h"
#include <optional>
#include <string>

namespace Glacier::sdk {

class Game {
public:
    static Game& get();

    // High-level accessors -----------------------------------------------------
    [[nodiscard]] ClientInstance*  clientInstance() const;
    [[nodiscard]] LocalPlayer*     localPlayer()    const;
    [[nodiscard]] Level*           level()          const;
    [[nodiscard]] Minecraft*       minecraft()      const;

    // Player queries (return nullopt while SDK is stubbed out) -----------------
    [[nodiscard]] std::optional<Vec3> playerPos()       const;
    [[nodiscard]] std::optional<Vec3> playerVelocity()  const;
    [[nodiscard]] std::optional<float> playerYaw()      const;
    [[nodiscard]] std::optional<float> playerPitch()    const;
    [[nodiscard]] std::optional<int>   playerHealth()   const;
    [[nodiscard]] std::optional<int>   playerHunger()   const;
    [[nodiscard]] std::optional<std::string> playerName() const;

    // World queries
    [[nodiscard]] std::optional<int> worldDayTime() const;
    [[nodiscard]] std::optional<int> chunkRenderDistance() const;

    // True if we have a non-stub local player hooked up.
    [[nodiscard]] bool inGame() const;
};

} // namespace Glacier::sdk
