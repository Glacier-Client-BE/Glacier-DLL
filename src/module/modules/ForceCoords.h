#pragma once

#include <array>
#include <cstring>
#include <format>

#include "../HudModule.h"
#include "../../memory/Memory.h"
#include "../../memory/SignatureManager.h"
#include "../../sdk/GameSDK.h"
#include "../../util/Logger.h"

namespace glacier {

// Forces the coordinate display on even when the server's "show coordinates"
// gamerule is off. Two layers:
//   1. Byte patch — the gamerule check compiles to `cmp byte [rax+x],y /
//      setne al`; replacing the setne with `mov al, 1` makes the vanilla HUD
//      always believe coordinates are enabled (Flarial's ForceCoordsOption).
//   2. Overlay readout — Glacier's own XYZ widget, drawn from the SDK position,
//      which works regardless of whether the patch signature resolved.
class ForceCoords final : public HudModule {
public:
    ForceCoords()
        : HudModule("Force Coords", "Force coordinate display when server disables it",
                    Category::World, /*positioned*/ false, "bottom-left") {
        addSetting(Setting{ "overlay", "Also show overlay XYZ", true });
    }

    ~ForceCoords() override { unpatch(); }

    void onEnable() override {
        if (m_patchSite == 0) {
            const auto sig = memory::SignatureManager::get().sig("ForceCoordsOption");
            if (sig != 0) {
                m_patchSite = sig + 4;   // the 3-byte `setne al` after the cmp
                std::memcpy(m_original.data(), reinterpret_cast<const void*>(m_patchSite), kPatchSize);
            } else {
                LOG_WARN("ForceCoordsOption signature not resolved — overlay fallback only");
            }
        }
        patch();
    }

    void onDisable() override { unpatch(); }

    void onRender() override {
        if (auto p = sdk::GameSDK::get().playerPosition()) {
            m_pos = *p;
            m_valid = true;
        } else {
            m_valid = false;
        }
    }

    bool hasHud() const override {
        const auto* s = setting("overlay");
        return s && s->asBool();
    }

protected:
    void writeHudBody(json::Object& o) const override {
        o.set("type", "text").set("id", "forcecoords").set("anchor", anchor())
         .set("label", "XYZ")
         .set("value", m_valid
                  ? std::format("{:.0f}, {:.0f}, {:.0f}", m_pos.x, m_pos.y, m_pos.z)
                  : "—")
         .set("sub", m_patched ? "forced (patched)" : "overlay only");
    }

private:
    static constexpr std::size_t kPatchSize = 3;
    // setne al (0F 95 C0) -> mov al, 1 ; nop — forces "coordinates enabled".
    static constexpr std::array<std::uint8_t, kPatchSize> kForcedOn{ 0xB0, 0x01, 0x90 };

    void patch() {
        if (m_patched || m_patchSite == 0) return;
        memory::ScopedProtect guard(reinterpret_cast<void*>(m_patchSite), kPatchSize, PAGE_EXECUTE_READWRITE);
        std::memcpy(reinterpret_cast<void*>(m_patchSite), kForcedOn.data(), kPatchSize);
        m_patched = true;
    }

    void unpatch() {
        if (!m_patched || m_patchSite == 0) return;
        memory::ScopedProtect guard(reinterpret_cast<void*>(m_patchSite), kPatchSize, PAGE_EXECUTE_READWRITE);
        std::memcpy(reinterpret_cast<void*>(m_patchSite), m_original.data(), kPatchSize);
        m_patched = false;
    }

    std::uintptr_t m_patchSite = 0;
    std::array<std::uint8_t, kPatchSize> m_original{};
    bool m_patched = false;

    sdk::Vec3 m_pos{};
    bool      m_valid = false;
};

} // namespace glacier
