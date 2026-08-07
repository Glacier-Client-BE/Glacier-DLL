#include "Config.h"

#include "../module/Module.h"
#include "../module/ModuleManager.h"
#include "../util/Logger.h"

#include <ShlObj.h>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace glacier {

namespace {

std::string trim(std::string_view s) {
    const auto notSpace = [](char c) {
        return c != ' ' && c != '\t' && c != '\r' && c != '\n';
    };
    std::size_t b = 0;
    while (b < s.size() && !notSpace(s[b])) ++b;
    std::size_t e = s.size();
    while (e > b && !notSpace(s[e - 1])) --e;
    return std::string{ s.substr(b, e - b) };
}

// Parsers return false on malformed input so the caller can keep the default
// and report exactly which key was bad.
bool parseBool(std::string_view s, bool& out) {
    if (s == "true"  || s == "1") { out = true;  return true; }
    if (s == "false" || s == "0") { out = false; return true; }
    return false;
}

bool parseInt(std::string_view s, int& out) {
    // from_chars has no locale dependency and no exceptions — both matter
    // inside an injected DLL where the host's locale is not ours to assume.
    const auto* first = s.data();
    const auto* last  = s.data() + s.size();
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        first += 2;
        base = 16;
    }
    const auto res = std::from_chars(first, last, out, base);
    return res.ec == std::errc{} && res.ptr == last;
}

bool parseUInt(std::string_view s, std::uint32_t& out) {
    const auto* first = s.data();
    const auto* last  = s.data() + s.size();
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        first += 2;
        base = 16;
    }
    const auto res = std::from_chars(first, last, out, base);
    return res.ec == std::errc{} && res.ptr == last;
}

bool parseFloat(std::string_view s, float& out) {
    const auto res = std::from_chars(s.data(), s.data() + s.size(), out);
    return res.ec == std::errc{} && res.ptr == s.data() + s.size();
}

// Applies one key/value pair to a module. Unknown keys are ignored by design.
void applyKey(Module& module, const std::string& key, const std::string& value) {
    if (key == "enabled") {
        bool v = false;
        if (parseBool(value, v)) {
            // Goes through setEnabled so onEnable/onDisable actually run — a
            // module restored as "on" must install its hooks/overrides, not
            // just show a lit toggle.
            module.setEnabled(v);
        } else {
            LOG_WARN("config: bad bool for {}.enabled = '{}'", module.name(), value);
        }
        return;
    }
    if (key == "keybind") {
        int v = 0;
        if (parseInt(value, v)) module.setKeybind(v);
        else LOG_WARN("config: bad keybind for {} = '{}'", module.name(), value);
        return;
    }

    Setting* setting = module.setting(key);
    if (!setting) return;   // setting removed in a newer build — ignore quietly

    switch (setting->type()) {
        case SettingType::Bool: {
            bool v = false;
            if (parseBool(value, v)) setting->set(v);
            else LOG_WARN("config: bad bool for {}.{} = '{}'", module.name(), key, value);
            break;
        }
        case SettingType::Float: {
            float v = 0;
            if (parseFloat(value, v)) setting->set(v);
            else LOG_WARN("config: bad float for {}.{} = '{}'", module.name(), key, value);
            break;
        }
        case SettingType::Int:
        case SettingType::Key: {
            int v = 0;
            if (parseInt(value, v)) setting->set(v);
            else LOG_WARN("config: bad int for {}.{} = '{}'", module.name(), key, value);
            break;
        }
        case SettingType::Color: {
            std::uint32_t v = 0;
            if (parseUInt(value, v)) setting->setColor(v);
            else LOG_WARN("config: bad color for {}.{} = '{}'", module.name(), key, value);
            break;
        }
    }
}

} // namespace

std::string Config::directory() {
    PWSTR roaming = nullptr;
    std::string base;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))
        && roaming) {
        char buf[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, roaming, -1, buf, sizeof(buf), nullptr, nullptr);
        base = buf;
    }
    if (roaming) CoTaskMemFree(roaming);

    if (base.empty()) base = ".";
    return base + "\\Glacier";
}

std::string Config::path() {
    return directory() + "\\config.ini";
}

bool Config::load() {
    m_loaded = true;   // set regardless: a missing config is a valid first run

    const std::string file = path();
    std::ifstream in(file);
    if (!in) {
        LOG_INFO("no config at {} — using defaults", file);
        return false;
    }

    auto& modules = ModuleManager::get();
    Module* current = nullptr;
    std::string line;
    int applied = 0;
    int unknownSections = 0;

    while (std::getline(in, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            const std::string name = trim(std::string_view{ trimmed }.substr(1, trimmed.size() - 2));
            current = modules.find(name);
            if (!current) ++unknownSections;
            continue;
        }

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        if (!current) continue;   // key outside any section

        applyKey(*current, trim(std::string_view{ trimmed }.substr(0, eq)),
                 trim(std::string_view{ trimmed }.substr(eq + 1)));
        ++applied;
    }

    LOG_INFO("config loaded from {} ({} values{})", file, applied,
             unknownSections ? ", " + std::to_string(unknownSections) + " unknown section(s) ignored"
                             : "");
    return true;
}

bool Config::save() {
    // Refusing to save before the initial load is what stops a crash-on-startup
    // from replacing a good config with a file full of defaults.
    if (!m_loaded) return false;

    std::error_code ec;
    std::filesystem::create_directories(directory(), ec);
    if (ec) {
        LOG_ERROR("config: could not create {}: {}", directory(), ec.message());
        return false;
    }

    const std::string target = path();
    const std::string temp   = target + ".tmp";

    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            LOG_ERROR("config: could not open {} for writing", temp);
            return false;
        }

        out << "# Glacier configuration\n"
            << "# Rewritten automatically; edits are preserved for known keys only.\n\n";

        for (const auto& module : ModuleManager::get().modules()) {
            out << '[' << module->name() << "]\n";
            out << "enabled = " << (module->enabled() ? "true" : "false") << '\n';
            out << "keybind = " << module->keybind() << '\n';

            for (const auto& setting : module->settings()) {
                out << setting.id() << " = ";
                switch (setting.type()) {
                    case SettingType::Bool:
                        out << (setting.asBool() ? "true" : "false");
                        break;
                    case SettingType::Float: {
                        // Fixed 6dp: enough to round-trip a normalized HUD
                        // position at 4K without drifting a pixel per save.
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "%.6f", setting.asFloat());
                        out << buf;
                        break;
                    }
                    case SettingType::Int:
                    case SettingType::Key:
                        out << setting.asInt();
                        break;
                    case SettingType::Color: {
                        char buf[16];
                        std::snprintf(buf, sizeof(buf), "0x%08X", setting.asColor());
                        out << buf;
                        break;
                    }
                }
                out << '\n';
            }
            out << '\n';
        }

        if (!out) {
            LOG_ERROR("config: write to {} failed", temp);
            return false;
        }
    }

    // Rename over the target. A crash before this point leaves the previous
    // config untouched rather than half-written.
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        // rename can fail across the temp/target boundary on some setups; fall
        // back to a copy so a save is never silently lost.
        std::filesystem::copy_file(temp, target,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(temp, ec);
        if (ec) {
            LOG_ERROR("config: could not commit {}: {}", target, ec.message());
            return false;
        }
    }

    LOG_INFO("config saved to {}", target);
    return true;
}

} // namespace glacier
