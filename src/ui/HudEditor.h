#pragma once

#include <string>

#include "Renderer.h"

// Drag-to-position for HUD widgets.
//
// While the menu is open, every enabled HUD module reports the rectangle it
// drew to; this class outlines them, hit-tests the cursor, and writes back the
// new normalized position when one is dragged. HUD modules therefore get
// repositioning for free — they never handle input themselves.
//
// The grab offset is captured on mouse-down and held for the whole drag, so the
// widget doesn't snap its top-left corner to the cursor when you grab it by the
// middle. Positions are written normalized (0..1) so they survive a resolution
// change, and clamped so a widget can't be dragged fully off-screen and lost.
namespace glacier::ui {

class HudEditor {
public:
    static HudEditor& get() {
        static HudEditor instance;
        return instance;
    }

    // True while the menu is open — HUD modules check this to decide whether to
    // draw their drag outline.
    bool active() const { return m_active; }
    void setActive(bool active) {
        m_active = active;
        if (!active) m_dragging.clear();
    }

    // Called by HudModule::onRender with the bounds it just drew. Returns the
    // new normalized position if this widget is being dragged, or nullopt.
    // `outX`/`outY` are the widget's current normalized position.
    bool update(const std::string& moduleName, const Rect& bounds,
                float& outX, float& outY);

    bool draggingAnything() const { return !m_dragging.empty(); }

private:
    HudEditor() = default;

    bool        m_active = false;
    std::string m_dragging;
    float       m_grabDx = 0;
    float       m_grabDy = 0;
};

} // namespace glacier::ui
