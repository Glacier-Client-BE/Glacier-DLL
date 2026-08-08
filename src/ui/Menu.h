#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>

#include "Renderer.h"
#include "../module/Module.h"

// The in-game menu.
//
// Immediate mode: there is no retained widget tree, so the menu can never drift
// out of sync with the module list it renders. Each frame walks the modules,
// draws them, and hit-tests against the same rectangles it just drew. State
// that must persist between frames (which tab is up, what's typed in the
// search box, which module's settings are open) lives here and is deliberately
// small.
//
// Layout is a card grid rather than a list, filtered by a search box and a row
// of category chips, with per-module settings opening as a full-width detail
// page rather than an inline expander. The previous list-with-expanders design
// made the panel's height depend on which rows happened to be open, which is
// why nothing in it could be laid out on a grid.
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

    // True while a keybind widget is waiting for a key. The WndProc hook checks
    // this so the captured key toggles nothing on its way through.
    bool capturingKey() const { return m_capturingModule != nullptr; }

    // True while the search box has focus and is swallowing typed characters.
    // The menu-toggle poll checks this: G and M are both perfectly ordinary
    // letters to type into a search box, and without this, typing "gamma"
    // would close the menu on its first keystroke.
    bool capturingText() const { return m_searchFocused; }

private:
    Menu() = default;

    enum class Tab { Modules, Performance };

    // ── Chrome ──
    void drawHeader(const Rect& area);
    void drawToolbar(const Rect& area);
    void drawModulesTab(const Rect& area);
    void drawPerformanceTab(const Rect& area);

    // ── Modules tab ──
    void drawGrid(const Rect& area);
    void drawCard(Module& module, const Rect& card);
    void drawDetail(const Rect& area, Module& module);
    void drawSettingRow(Module& module, Setting& setting, Rect& cursor, float width);

    // ── Shared widgets. Each returns true if it changed something. ──
    bool widgetToggle(const Rect& r, bool value);
    bool widgetSlider(const Rect& r, Setting& setting);
    bool widgetKeybind(const Rect& r, Module& module);
    // Returns the extra vertical space consumed by the expanded channel sliders
    // (0 when collapsed), so the caller can advance its layout cursor.
    float widgetColor(const Rect& swatch, Setting& setting);
    // Pill-shaped text button, used for the tab strip and the category chips.
    bool widgetChip(const Rect& r, std::string_view label, bool selected);
    // Circular icon button. Falls back to a drawn glyph-less disc if the icon
    // font is unavailable, so the control is never invisible.
    bool widgetIconButton(const Rect& r, wchar_t glyph, bool emphasised = false);

    // Draws a module's mark: its Font Awesome icon, its category's icon if it
    // declared none, or a lettered tile if the icon font failed to load. The
    // last of those is why nothing here ever draws a raw codepoint blind.
    void drawModuleMark(const Module& module, const Rect& box, const Color& color,
                        float size);

    void drawScrollbar(const Rect& track, float scroll, float contentH, float viewH);

    bool hovered(const Rect& r) const;
    bool clicked(const Rect& r) const;

    // Whether `module` survives the current search text and category chip.
    bool matchesFilter(const Module& module) const;

    bool m_open = false;
    Tab  m_tab  = Tab::Modules;
    Rect m_panelRect{};

    // -1 means "All"; otherwise a Category cast to int. An int rather than an
    // optional<Category> because it is also the chip index.
    int   m_categoryFilter = -1;
    float m_scroll = 0.0f;

    std::string m_search;
    bool        m_searchFocused = false;
    // Last-drawn search field, so a click anywhere else can drop its focus.
    Rect        m_searchRect{};

    // Which module's detail page is open, by name — pointers would dangle if
    // the module list were ever rebuilt. Empty means the grid is showing.
    std::string m_detail;

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

    // Frame-rate history for the Performance tab, sampled on a timer rather
    // than per frame so the graph covers a useful span instead of the last
    // two seconds.
    static constexpr int kGraphSamples = 120;
    int       m_fpsHistory[kGraphSamples]{};
    int       m_fpsCount = 0;
    ULONGLONG m_lastSample = 0;
};

} // namespace glacier::ui
