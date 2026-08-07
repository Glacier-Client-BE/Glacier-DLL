#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

// Drawing real item icons.
//
// Why this is not part of ui::Renderer
// ------------------------------------
// Glacier's overlay owns a private D3D device and composites at Present. Item
// textures live in the game's atlas, on the game's device, and are only bound
// during the game's own UI pass — so there is no way to sample them from our
// renderer. The only practical route is to let the game draw the icons for us,
// from inside a game render call.
//
// So this does two things:
//
//   1. Hooks ScreenView::setupAndRender, which is a point inside the game's UI
//      pass where a live MinecraftUIRenderContext is in hand.
//   2. Accepts draw requests from HUD modules during Glacier's own overlay
//      pass, and replays them from inside that hook.
//
// Two consequences fall out of the split, and both are deliberate:
//
//   * Icons are drawn one frame behind the widget that requested them. The UI
//     pass of frame N+1 replays what the overlay pass of frame N asked for.
//     For a HUD widget that moves only when dragged, this is invisible.
//
//   * Icons are drawn UNDER Glacier's overlay, because the overlay composites
//     later in the frame. A HUD widget that fills a background where an icon
//     goes will hide it — widgets must leave a hole, not paint over one.
namespace glacier::sdk {

// Which item to draw. Deliberately a *description* rather than an ItemStack
// pointer: the request is made in one pass and honoured in another, and a raw
// game pointer captured across that gap is exactly the kind of thing that is
// fine until the player opens a chest. The pointer is resolved fresh, inside
// the pass that draws it.
struct ItemRef {
    enum class Source : std::uint8_t {
        Armor,       // index 0..3, helmet -> boots
        Hotbar,      // index 0..8, or -1 for whatever is currently selected
    };

    Source source = Source::Armor;
    int    index  = 0;
};

// One queued icon. Position and size are in swapchain pixels — the same space
// HUD modules already lay out in — and are converted to the game's GUI units
// at draw time.
struct ItemDraw {
    ItemRef ref;
    float   x = 0.0f;
    float   y = 0.0f;
    float   size = 48.0f;
    float   opacity = 1.0f;
};

class ItemRendering {
public:
    static ItemRendering& get() {
        static ItemRendering instance;
        return instance;
    }

    // On by default, overridable with `itemIcons = false` under [Glacier] in
    // config.ini.
    //
    // It was briefly defaulted off while the world-load crash was unexplained.
    // That crash turned out to have a separate, confirmed cause (Glacier was
    // hooking its own debug console — see docs/HANDOFF.md), and the calls into
    // game code here are wrapped in structured-exception guards that switch the
    // feature off and name what failed. The config key stays as the one-line
    // recovery if this path ever does turn out to be the unstable one.
    //
    // Must be called before installHooks() to have any effect.
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

    // Installs the ScreenView hook, if enabled. Safe to call when the
    // signatures are missing — it logs what is unavailable and every later
    // submit() becomes a no-op, so HUD widgets degrade to "no icons" rather
    // than misbehaving.
    void installHooks();

    // True when every signature the draw path needs resolved. HUD modules query
    // this to decide whether to lay out space for an icon at all.
    bool available() const { return m_available; }

    // Called from Glacier's overlay pass. Cheap: appends to a vector.
    void submit(const ItemDraw& draw);

    // Ends the overlay pass's batch and hands it to the game pass. Called once
    // per overlay frame, after every module has rendered.
    void publish();

    // Called by the ScreenView hook. Not private because the hook is a free
    // function; not part of the public contract otherwise.
    void drawPending(void* uiRenderContext);

private:
    ItemRendering() = default;

    // Turns the feature off for the rest of the session after a fault, naming
    // what failed. Called from the render path, so it must not throw.
    void disableAfterFault(const char* what);

    bool m_enabled = true;
    bool m_available = false;

    // Two batches: one being filled by the overlay pass, one ready for the game
    // pass to consume. A mutex rather than lock-free anything — this is touched
    // twice a frame with a handful of elements, and the two passes are very
    // likely the same thread anyway.
    std::mutex            m_mutex;
    std::vector<ItemDraw> m_building;
    std::vector<ItemDraw> m_ready;
};

} // namespace glacier::sdk
