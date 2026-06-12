#pragma once

#include <Ultralight/Ultralight.h>
#include <Ultralight/View.h>
#include <AppCore/JSHelpers.h>

#include <string>

// The native <-> JavaScript bridge. On every page load it installs a global
// `glacier` object whose methods are backed by C++ lambdas. The web UI calls
// these to read module state and push user actions (toggles, slider changes,
// keybind edits) back into the client core. State flows back to JS by invoking
// a `window.glacier.onState(json)` callback the page registers.
//
// All bound functions execute on Ultralight's update thread (the Present hook),
// so they take the ModuleManager mutex before touching shared state.
namespace glacier {

class JsBridge : public ultralight::LoadListener {
public:
    explicit JsBridge(ultralight::RefPtr<ultralight::View> view);
    ~JsBridge() override;

    // LoadListener — re-bind the API whenever the DOM is (re)created.
    void OnDOMReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame,
                    const ultralight::String& url) override;

    // Pushes the full module/settings snapshot to the page (called on open and
    // after any change so the UI stays authoritative-from-C++).
    void pushState();

    // Pushes the always-on HUD payload (FPS/CPS/armor/inventory…). Called on a
    // throttle from the render loop, independent of whether the menu is open.
    void pushHud();

    // Tells the page to show/hide the click-GUI without unloading it (the HUD
    // keeps rendering underneath).
    void pushMenuVisible(bool visible);

private:
    // ── Native functions exposed to JS ──
    // glacier.getState()            -> JSON string of all modules + settings
    // glacier.toggleModule(name)    -> bool (new enabled state)
    // glacier.setSetting(mod,id,v)  -> void
    // glacier.setKeybind(mod,vk)    -> void
    // glacier.closeMenu()           -> void
    ultralight::JSValue jsGetState(const ultralight::JSObject&, const ultralight::JSArgs&);
    ultralight::JSValue jsToggleModule(const ultralight::JSObject&, const ultralight::JSArgs&);
    ultralight::JSValue jsSetSetting(const ultralight::JSObject&, const ultralight::JSArgs&);
    ultralight::JSValue jsSetKeybind(const ultralight::JSObject&, const ultralight::JSArgs&);
    ultralight::JSValue jsCloseMenu(const ultralight::JSObject&, const ultralight::JSArgs&);

    // Serializes the live module list to a JSON document the page can render.
    std::string buildStateJson() const;

    ultralight::RefPtr<ultralight::View> m_view;
};

} // namespace glacier
