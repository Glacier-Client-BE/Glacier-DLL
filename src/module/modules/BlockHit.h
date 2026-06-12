#pragma once

#include <Windows.h>

#include "../Module.h"

namespace glacier {

// Java-style block-hit feedback: a quick crosshair-centred pulse on every left
// click, so swings have visible weight. The module records the click time; the
// HUD payload carries an animation progress value and the web layer draws the
// expanding ring (no per-frame allocation, one timestamp).
class BlockHit final : public Module {
public:
    BlockHit()
        : Module("Block Hit", "Visual block-hit animation overlay", Category::Visual) {
        addSetting(Setting{ "duration", "Duration (s)", 0.25f, 0.10f, 1.00f, 0.05f });
        addSetting(Setting{ "size", "Size", 1.0f, 0.5f, 3.0f, 0.1f });
    }

    void onClick(bool rightButton) override {
        if (rightButton) return;
        m_hitStart = GetTickCount64();
    }

    bool hasHud() const override { return true; }

    void writeHud(json::Array& huds) const override {
        if (m_hitStart == 0) return;

        float duration = 0.25f;
        if (const auto* s = setting("duration")) duration = s->asFloat();
        float size = 1.0f;
        if (const auto* s = setting("size")) size = s->asFloat();

        const double elapsed = static_cast<double>(GetTickCount64() - m_hitStart) / 1000.0;
        if (elapsed >= duration) return;

        json::Object o;
        o.set("type", "pulse").set("id", "blockhit")
         .set("progress", elapsed / duration)
         .set("size", size);
        huds.push(o);
    }

private:
    unsigned long long m_hitStart = 0;
};

} // namespace glacier
