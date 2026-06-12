#pragma once

#include <string>

#include "Module.h"

namespace glacier {

// Base for always-on HUD widgets. Inspired by Latite's HUDModule, which bakes
// position + scale into the base so individual widgets don't re-declare them.
//
// Two flavours:
//   • "positioned"  → free x/y placement + scale (armor, inventory grids).
//   • "anchored"    → docked to a screen corner (FPS/CPS text readouts).
//
// Subclasses implement writeHudBody() to add their `type` + content; the base
// appends the shared positioning fields and pushes the object.
class HudModule : public Module {
public:
    HudModule(std::string name, std::string description, Category category,
              bool positioned, std::string anchor = "top-right",
              int defaultX = 8, int defaultY = 8, int keybind = 0)
        : Module(std::move(name), std::move(description), category, keybind),
          m_positioned(positioned), m_anchor(std::move(anchor)) {
        if (positioned) {
            addSetting(Setting{ "x",     "X position", defaultX, 0, 1920 });
            addSetting(Setting{ "y",     "Y position", defaultY, 0, 1080 });
            addSetting(Setting{ "scale", "Scale", 1.0f, 0.5f, 3.0f, 0.1f });
        }
    }

    bool hasHud() const override { return true; }

    void writeHud(json::Array& huds) const override {
        json::Object o;
        writeHudBody(o);
        if (m_positioned) {
            o.set("x", x()).set("y", y()).set("scale", scale());
        }
        huds.push(o);
    }

    int   x() const     { const auto* s = setting("x");     return s ? s->asInt() : 8; }
    int   y() const     { const auto* s = setting("y");     return s ? s->asInt() : 8; }
    float scale() const { const auto* s = setting("scale"); return s ? s->asFloat() : 1.0f; }
    const std::string& anchor() const { return m_anchor; }

protected:
    // Fill `o` with the widget's "type" and content fields. Position/scale are
    // added by the base for positioned widgets.
    virtual void writeHudBody(json::Object& o) const = 0;

private:
    bool        m_positioned;
    std::string m_anchor;
};

} // namespace glacier
