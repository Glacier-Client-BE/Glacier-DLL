#pragma once
//
// Top-level glue. Owns: Logger lifecycle, Hook init, Renderer init, default
// module registration, Config load/save. Lifetime: from DllMain attach to
// detach (or explicit Eject).
//
#include <atomic>

namespace Glacier {

class Client {
public:
    static Client& get();

    void start();   // Called once from the bootstrap thread in dllmain.
    void stop();    // Disable hooks, save config.

    [[nodiscard]] bool running() const { return running_.load(); }

private:
    Client() = default;
    std::atomic<bool> running_{ false };
    void registerModules();
};

} // namespace Glacier
