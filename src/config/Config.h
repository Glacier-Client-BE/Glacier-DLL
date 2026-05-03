#pragma once
//
// Config = JSON file under %appdata%\Glacier\config\<profile>.json.
// Persists every Module and the active profile name.
//
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Glacier {

class Config {
public:
    static Config& get();

    // Active profile name (default: "default").
    [[nodiscard]] const std::string& profile() const { return profile_; }
    void setProfile(const std::string& p) { profile_ = p; }

    // Disk paths
    [[nodiscard]] std::wstring rootDir() const;
    [[nodiscard]] std::wstring profilePath(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> listProfiles() const;

    // Save / load the active profile.
    void save() const;
    void load();
    bool deleteProfile(const std::string& name) const;

private:
    Config() = default;
    std::string profile_ = "default";
};

} // namespace Glacier
