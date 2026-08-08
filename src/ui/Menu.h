#pragma once

#include <string>
#include <vector>

#include "Renderer.h"
#include "../module/Module.h"

// The in-game menu.
//
// Immediate mode: there is no retained widget tree, so the menu can never drift
// out of sync with the module list it renders. Each frame walks the modules,
// draws them, and hit-tests against the same rectangles it just drew. State
// that must persist between frames (which category is selected, which module is
// expanded, which keybind is capturing) lives here and is deliberately small.
namespace glacier::ui {

class Menu {
public:
    static Menu& get() {
        static Menu instance;
        return instance;
    }

    void render();

    bool open() const { return m_open; }
    void setOpen(bool open);

    // The panel's last-drawn screen rect, so HudEditor can decline to drag a
    // HUD widget positioned underneath it — otherwise a click meant for a
    // menu button that happens to land on a hidden widget grabs that widget
    // instead (or as well as) clicking the button. Stale after close, but
    // harmless: HudEditor only ever consults this while the menu is active.
    Rect panelRect() const { return m_panelRect; }
    void toggle() {
        static ULONGLONG lastToggle = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastToggle < 50) return;
        lastToggle = now;
        setOpen(!m_open);
    }

    // True while a keybind widget is waiting for a key. The WndProc hook checks
    // this so the captured key toggles nothing on its way through.
    bool capturingKey() const { return m_capturingModule != nullptr; }

private:
    Menu() = default;

    void drawSidebar(const Rect& area);
    void drawModuleList(const Rect& area);
    void drawModuleCard(Module& module, Rect& cursor, float width);
    void drawSettings(Module& module, Rect& cursor, float width);
    void drawCursor(float x, float y);

    // Widgets. Each returns true if it changed the underlying value this frame.
    bool widgetToggle(const Rect& r, bool value);
    bool widgetSlider(const Rect& r, Setting& setting);
    bool widgetKeybind(const Rect& r, Module& module);
    // Returns the extra vertical space consumed by the expanded channel sliders
    // (0 when collapsed), so the caller can advance its layout cursor.
    float widgetColor(const Rect& swatch, Setting& setting, float rowWidth);

    bool hovered(const Rect& r) const;
    bool clicked(const Rect& r) const;

    bool     m_open = false;
    Category m_category = Category::Visual;
    float    m_scroll = 0.0f;
    Rect     m_panelRect{};

    // Which module's settings are expanded (by name — pointers would dangle if
    // the module list were ever rebuilt).
    std::string m_expanded;

    // Non-null while a keybind widget is capturing.
    Module* m_capturingModule = nullptr;

    // The slider currently being dragged. Tracked explicitly so dragging keeps
    // working when the cursor leaves the slider's rectangle mid-drag.
    Setting* m_draggingSetting = nullptr;

    // The color swatch whose channel sliders are expanded.
    Setting* m_expandedColor = nullptr;

    // Channel index (0=A,1=R,2=G,3=B) currently being dragged, -1 for none.
    // Channels can't reuse m_draggingSetting because all four share one Setting.
    int m_draggingChannel = -1;
};

} // namespace glacier::ui
