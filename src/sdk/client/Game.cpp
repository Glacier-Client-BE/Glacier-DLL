#include "Game.h"
#include "../Addresses.h"

namespace Glacier::sdk {

Game& Game::get() {
    static Game G;
    return G;
}

// All accessors short-circuit while the addresses are unresolved.
ClientInstance* Game::clientInstance() const {
    auto& A = Addresses::get();
    if (!A.ready() || !A.clientInstance) return nullptr;
    // *(ClientInstance**)A.clientInstance after RE - returning nullptr for now.
    return nullptr;
}

LocalPlayer*  Game::localPlayer() const { return nullptr; }
Level*        Game::level()       const { return nullptr; }
Minecraft*    Game::minecraft()   const { return nullptr; }

std::optional<Vec3>  Game::playerPos()      const { return std::nullopt; }
std::optional<Vec3>  Game::playerVelocity() const { return std::nullopt; }
std::optional<float> Game::playerYaw()      const { return std::nullopt; }
std::optional<float> Game::playerPitch()    const { return std::nullopt; }
std::optional<int>   Game::playerHealth()   const { return std::nullopt; }
std::optional<int>   Game::playerHunger()   const { return std::nullopt; }
std::optional<std::string> Game::playerName() const { return std::nullopt; }

std::optional<int> Game::worldDayTime()        const { return std::nullopt; }
std::optional<int> Game::chunkRenderDistance() const { return std::nullopt; }

bool Game::inGame() const { return false; }

} // namespace Glacier::sdk
