#include "HudEditor.h"

#include "Input.h"

#include <algorithm>

namespace glacier::ui {

bool HudEditor::update(const std::string& moduleName, const Rect& bounds,
                       float& outX, float& outY) {
    if (!m_active || bounds.w <= 0.0f || bounds.h <= 0.0f) return false;

    auto& r  = Renderer::get();
    auto& in = Input::get();

    // Padded grab area — a tight text bounding box is fiddly to hit.
    const Rect grab = bounds.inset(-4.0f);
    const bool over = grab.contains(in.mouseX(), in.mouseY());
    const bool isDragging = (m_dragging == moduleName);

    // Outline so the user can see what is movable.
    r.strokeRoundedRect(grab, 3.0f,
                        isDragging ? Color::rgba(0xFF4C9AFF)
                                   : Color::rgba(over ? 0xAA4C9AFF : 0x556E7891),
                        isDragging ? 1.5f : 1.0f);

    // Claim the drag. Only one widget can be dragging at a time, so a stack of
    // overlapping HUDs doesn't all move together.
    if (over && in.leftPressed() && m_dragging.empty()) {
        m_dragging = moduleName;
        m_grabDx = in.mouseX() - bounds.x;
        m_grabDy = in.mouseY() - bounds.y;
        return false;
    }

    if (!isDragging) return false;

    if (!in.leftDown()) {
        m_dragging.clear();
        return false;
    }

    const float screenW = r.width();
    const float screenH = r.height();
    if (screenW <= 0.0f || screenH <= 0.0f) return false;

    // Clamp in pixels against the widget's own size so it can always be grabbed
    // again — normalizing first would let a wide widget hang off the edge.
    const float maxX = std::max(0.0f, screenW - bounds.w);
    const float maxY = std::max(0.0f, screenH - bounds.h);
    const float px = std::clamp(in.mouseX() - m_grabDx, 0.0f, maxX);
    const float py = std::clamp(in.mouseY() - m_grabDy, 0.0f, maxY);

    outX = px / screenW;
    outY = py / screenH;
    return true;
}

} // namespace glacier::ui
