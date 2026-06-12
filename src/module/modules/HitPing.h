#pragma once

#include <format>
#include <Windows.h>

#include "../Module.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Shows your network ping the moment you land a hit. The GameMode::attack hook
// bumps an attack sequence number; when it changes, the module snapshots the
// RakPeer ping cache and shows a fading popup near the crosshair.
class HitPing final : public Module {
public:
    HitPing()
        : Module("Hit Ping", "Show your network ping when you land a hit", Category::Combat) {
        addSetting(Setting{ "duration", "Hold time (s)", 1.5f, 0.5f, 5.0f, 0.25f });
    }

    void onRender() override {
        const auto attack = sdk::GameSDK::get().lastAttack();
        if (attack.valid && attack.seq != m_lastSeq) {
            m_lastSeq   = attack.seq;
            m_shownAt   = GetTickCount64();
            m_shownPing = sdk::GameSDK::get().ping();
        }
    }

    bool hasHud() const override { return true; }

    void writeHud(json::Array& huds) const override {
        if (m_shownAt == 0) return;

        float duration = 1.5f;
        if (const auto* s = setting("duration")) duration = s->asFloat();

        const double elapsed = static_cast<double>(GetTickCount64() - m_shownAt) / 1000.0;
        if (elapsed >= duration) return;

        json::Object o;
        o.set("type", "popup").set("id", "hitping")
         .set("value", m_shownPing >= 0 ? std::format("{} ms", m_shownPing) : "— ms")
         .set("progress", elapsed / duration);
        huds.push(o);
    }

private:
    std::uint32_t      m_lastSeq   = 0;
    unsigned long long m_shownAt   = 0;
    int                m_shownPing = -1;
};

} // namespace glacier
