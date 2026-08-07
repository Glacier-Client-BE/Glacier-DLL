#include "GameVersion.h"

#include "../util/Logger.h"

#include <Windows.h>
#include <format>
#include <vector>

#pragma comment(lib, "Version.lib")

namespace glacier::memory {

namespace {

GameVersion read() {
    GameVersion v;

    // The main module is Minecraft.Windows.exe when injected into the game.
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
        LOG_WARN("could not determine game executable path — version unknown");
        return v;
    }

    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path, &handle);
    if (size == 0) {
        LOG_WARN("game executable has no version resource — version unknown");
        return v;
    }

    std::vector<std::byte> buffer(size);
    if (!GetFileVersionInfoW(path, handle, size, buffer.data())) {
        LOG_WARN("GetFileVersionInfo failed — version unknown");
        return v;
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<void**>(&info), &len)
        || !info || len == 0) {
        LOG_WARN("version resource has no fixed file info — version unknown");
        return v;
    }

    v.major = static_cast<int>(HIWORD(info->dwFileVersionMS));
    v.minor = static_cast<int>(LOWORD(info->dwFileVersionMS));
    v.patch = static_cast<int>(HIWORD(info->dwFileVersionLS));
    v.build = static_cast<int>(LOWORD(info->dwFileVersionLS));
    v.valid = true;
    return v;
}

} // namespace

std::string GameVersion::toString() const {
    if (!valid) return "unknown";
    return std::format("{}.{}.{}.{}", major, minor, patch, build);
}

bool GameVersion::atLeast(int maj, int min, int pat) const {
    if (!valid) return false;
    if (major != maj) return major > maj;
    if (minor != min) return minor > min;
    return patch >= pat;
}

const GameVersion& gameVersion() {
    static const GameVersion cached = read();
    return cached;
}

} // namespace glacier::memory
