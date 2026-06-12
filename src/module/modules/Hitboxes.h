#pragma once

#include <vector>

#include "../Module.h"
#include "../../sdk/GameSDK.h"
#include "../../util/Json.h"

namespace glacier {

// Entity hitbox visualizer (visual only — never alters the server-side hitbox).
//
// Ported from Flarial's Hitbox: nearby actors are enumerated via the engine's
// own thread-safe Level::getRuntimeActorList, each actor's AABB is projected to
// screen with the live camera matrices (GameSDK::projectHitboxes), and the
// resulting 3D box edges are drawn as lines on the web overlay's canvas. The
// projection runs on the render thread in onRender(); writeHud() (same thread,
// immediately after) serializes the cached result.
class Hitboxes final : public Module {
public:
    Hitboxes()
        : Module("Hitboxes", "Render entity hitbox outlines (visual only)", Category::Visual) {
        addSetting(Setting{ "range", "Range (blocks)", 30, 5, 64 });
        addSetting(Setting{ "width", "Line width", 2.0f, 0.5f, 5.0f, 0.5f });
    }

    void onRender() override {
        float range = 30.0f;
        if (const auto* s = setting("range")) range = static_cast<float>(s->asInt());
        m_boxes = sdk::GameSDK::get().projectHitboxes(range, 0.0f);
    }

    void onDisable() override { m_boxes.clear(); }

    bool hasHud() const override { return true; }

    void writeHud(json::Array& huds) const override {
        if (m_boxes.empty()) return;

        float width = 2.0f;
        if (const auto* s = setting("width")) width = s->asFloat();

        // Build the boxes array: each box is its 8 projected corners as
        // [x, y, valid]. The web layer joins them with the shared edge table.
        json::Array boxes;
        for (const auto& b : m_boxes) {
            json::Array corners;
            for (const auto& c : b.corners) {
                json::Array pt;
                pt.pushRaw(std::to_string(static_cast<int>(c.x + 0.5f)));
                pt.pushRaw(std::to_string(static_cast<int>(c.y + 0.5f)));
                pt.pushRaw(c.valid ? "1" : "0");
                corners.pushRaw(pt.str());
            }
            json::Object box;
            box.raw("c", corners.str()).set("sel", b.selected);
            boxes.push(box);
        }

        json::Object o;
        o.set("type", "hitbox3d").set("id", "hitbox3d").set("width", width);
        o.raw("boxes", boxes.str());
        huds.push(o);
    }

private:
    std::vector<sdk::ScreenBox> m_boxes;
};

} // namespace glacier
