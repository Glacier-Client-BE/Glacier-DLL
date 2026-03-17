#pragma once
#include <Windows.h>
#include <string>
#include <filesystem>

// ─────────────────────────────────────────────────────────────────────────────
//  ClientConfig — runtime configuration for Glacier Client.
//  Initialized once in Client::init(), then read from anywhere.
// ─────────────────────────────────────────────────────────────────────────────
class ClientConfig {
public:
    static ClientConfig& get() {
        static ClientConfig instance;
        return instance;
    }

    void init(HMODULE hModule) {
        char path[MAX_PATH]{};
        GetModuleFileNameA(hModule, path, MAX_PATH);
        dllDirectory = std::filesystem::path(path).parent_path().string();
        menuKey      = 'M';
    }

    // Returns an absolute path to a file sitting next to glacier.dll
    std::string resolvePath(const std::string& filename) const {
        return dllDirectory + "\\" + filename;
    }

    std::string dllDirectory;
    int         menuKey = 'M';   // Virtual-key code — configurable in mod menu

private:
    ClientConfig() = default;
    ClientConfig(const ClientConfig&) = delete;
    ClientConfig& operator=(const ClientConfig&) = delete;
};
