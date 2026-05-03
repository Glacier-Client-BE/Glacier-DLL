#include "Config.h"
#include "../core/Util.h"
#include "../core/Logger.h"
#include "../modules/ModuleManager.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Glacier {

Config& Config::get() {
    static Config C;
    return C;
}

std::wstring Config::rootDir() const {
    return Util::appDataPath(L"config");
}

std::wstring Config::profilePath(const std::string& name) const {
    auto root = rootDir();
    CreateDirectoryW(root.c_str(), nullptr);
    return root + L"\\" + Util::utf8ToWide(name) + L".json";
}

std::vector<std::string> Config::listProfiles() const {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(rootDir(), ec)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != L".json") continue;
        out.push_back(Util::wideToUtf8(e.path().stem().wstring()));
    }
    if (out.empty()) out.emplace_back("default");
    return out;
}

void Config::save() const {
    nlohmann::json root;
    root["profile"] = profile_;
    auto& mods = root["modules"];
    mods = nlohmann::json::object();
    for (auto& m : ModuleManager::get().all()) {
        nlohmann::json mj;
        m->serialize(mj);
        mods[m->id()] = std::move(mj);
    }
    auto path = profilePath(profile_);
    std::ofstream f(path);
    if (!f) {
        Logger::get().error("Config", "Failed to open ", Util::wideToUtf8(path), " for write");
        return;
    }
    f << root.dump(2);
    Logger::get().info("Config", "Saved ", Util::wideToUtf8(path));
}

void Config::load() {
    auto path = profilePath(profile_);
    std::ifstream f(path);
    if (!f) {
        Logger::get().info("Config", "No config at ", Util::wideToUtf8(path), " - keeping defaults");
        return;
    }
    nlohmann::json root;
    try { f >> root; }
    catch (const std::exception& ex) {
        Logger::get().error("Config", "Parse error: ", ex.what());
        return;
    }
    if (root.contains("profile")) profile_ = root["profile"].get<std::string>();
    if (root.contains("modules") && root["modules"].is_object()) {
        for (auto& m : ModuleManager::get().all()) {
            if (root["modules"].contains(m->id())) {
                m->deserialize(root["modules"][m->id()]);
            }
        }
    }
    Logger::get().info("Config", "Loaded ", Util::wideToUtf8(path));
}

bool Config::deleteProfile(const std::string& name) const {
    if (name == "default") return false;
    std::error_code ec;
    return fs::remove(profilePath(name), ec);
}

} // namespace Glacier
