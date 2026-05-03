#include "Game.h"
#include "../Addresses.h"

namespace Glacier::sdk {

Game& Game::get() {
    static Game G;
    return G;
}

// All accessors short-circuit while the addresses are unresolved.
//
// Latite reaches ClientInstance via MinecraftGame -> getPrimaryClientInstance,
// then LocalPlayer / Level off ClientInstance. Until those struct offsets are
// reverse-engineered into typed accessors, every getter returns nullopt and
// modules treat the absence as "not in-game".
//
ClientInstance* Game::clientInstance() const {
    auto& A = Addresses::get();
    if (!A.ready() || !A.misc.MinecraftGame || !A.fn.getPrimaryClient) return nullptr;
    // (*(ClientInstance*(__fastcall**)(MinecraftGame*))A.fn.getPrimaryClient)(*A.misc.MinecraftGame)
    // once the typed call is wired. nullptr until then.
    return nullptr;
}

LocalPlayer*  Game::localPlayer() const { return nullptr; }
Level*        Game::level()       const { return nullptr; }
Minecraft*    Game::minecraft()   const {
    auto& A = Addresses::get();
    if (!A.ready() || !A.misc.MinecraftGame) return nullptr;
    return reinterpret_cast<Minecraft*>(*static_cast<void**>(A.misc.MinecraftGame));
}

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
