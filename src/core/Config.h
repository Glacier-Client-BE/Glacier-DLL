#pragma once

#include <atomic>
#include <string>

// Settings persistence.
//
// ── Format ──
// Sectioned key/value, not JSON:
//
//     [Fullbright]
//     enabled = true
//     keybind = 66
//     gamma = 1.000000
//
// The config is inherently flat — module, setting id, value — so a JSON parser
// would be a lot of surface area (and a lot of failure modes) for zero
// structural benefit. This format is trivially parsed, survives hand-editing,
// and diffs cleanly.
//
// ── Robustness ──
// Unknown sections and keys are ignored rather than dropped-on-load: a config
// written by a newer build, or one mentioning a module that no longer exists,
// must not lose the rest of the file's settings. Unparseable values fall back
// to the module's default and log which key was bad, so a single typo can't
// prevent the client from starting.
//
// Writes go to a temp file and are then renamed over the target, so a crash
// mid-save leaves the previous config intact instead of a truncated one.
namespace glacier {

class Config {
public:
    static Config& get() {
        static Config instance;
        return instance;
    }

    // %APPDATA%\Glacier\config.ini — deliberately not next to the DLL, which
    // may be injected from a temp directory or from inside the ACL'd
    // WindowsApps tree where writes fail.
    static std::string path();

    // The folder holding the config — and now the log file, which is why this
    // is public: the logger has to be brought up before Config itself, so it
    // needs the location without depending on the rest of Config.
    static std::string directory();

    // Applies the stored values onto the live modules. Safe to call when no
    // config exists yet (first run) — that is not an error.
    bool load();

    // Serializes every module's state. Returns false only on a real I/O
    // failure, which is logged.
    bool save();

    // Set once the initial load has happened, so autosave can't run before the
    // modules have been populated and overwrite a good file with defaults.
    bool loaded() const { return m_loaded; }

    // Requests a save without performing one.
    //
    // Callers are the window thread (menu closed) and the render thread — file
    // I/O on either causes a visible hitch, so the actual write is deferred to
    // the logic loop, which calls flush() once per iteration.
    void markDirty() { m_dirty.store(true, std::memory_order_relaxed); }

    // Saves if dirty. Called from the logic thread.
    void flush() {
        if (m_dirty.exchange(false, std::memory_order_relaxed)) {
            save();
        }
    }

private:
    Config() = default;

    bool m_loaded = false;
    std::atomic<bool> m_dirty{ false };
};

} // namespace glacier
