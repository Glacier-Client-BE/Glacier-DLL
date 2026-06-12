#pragma once

#include <array>
#include <cstring>

#include "../Module.h"
#include "../../memory/Memory.h"
#include "../../memory/SignatureManager.h"
#include "../../util/Logger.h"

namespace glacier {

// Removes the camera-shake component of view bobbing by NOPing the 6-byte
// indirect call that applies it (Flarial's MinimalViewBobbing patch). The
// original bytes are saved on first enable and restored on disable, so the
// module toggles cleanly and the DLL detaches without leaving patches behind.
class MinimalViewBobbing final : public Module {
public:
    MinimalViewBobbing()
        : Module("Minimal View Bobbing", "Reduced view-bobbing intensity", Category::Visual) {}

    ~MinimalViewBobbing() override { unpatch(); }

    void onEnable() override {
        if (m_address == 0) {
            m_address = memory::SignatureManager::get().sig("MinimalViewBobbing");
            if (m_address == 0) {
                LOG_WARN("MinimalViewBobbing signature not resolved — patch skipped");
                return;
            }
            std::memcpy(m_original.data(), reinterpret_cast<const void*>(m_address), kPatchSize);
        }
        patch();
    }

    void onDisable() override { unpatch(); }

private:
    static constexpr std::size_t kPatchSize = 6;   // FF 15 xx xx xx xx (call [rip+n])

    void patch() {
        if (m_patched || m_address == 0) return;
        memory::ScopedProtect guard(reinterpret_cast<void*>(m_address), kPatchSize, PAGE_EXECUTE_READWRITE);
        std::memset(reinterpret_cast<void*>(m_address), 0x90, kPatchSize);
        m_patched = true;
    }

    void unpatch() {
        if (!m_patched || m_address == 0) return;
        memory::ScopedProtect guard(reinterpret_cast<void*>(m_address), kPatchSize, PAGE_EXECUTE_READWRITE);
        std::memcpy(reinterpret_cast<void*>(m_address), m_original.data(), kPatchSize);
        m_patched = false;
    }

    std::uintptr_t m_address = 0;
    std::array<std::uint8_t, kPatchSize> m_original{};
    bool m_patched = false;
};

} // namespace glacier
