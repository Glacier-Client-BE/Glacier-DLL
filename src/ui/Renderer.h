#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dwrite.h>
#include <dxgi.h>

// Direct2D/DirectWrite overlay renderer.
//
// Draws straight onto the game's back buffer, the way Flarial does:
// D2D1CreateDeviceContext binds the swap chain's DXGI surface directly, on the
// game's own device.
//
// This replaced a private-device design — a second D3D11 device rendering into
// a shared keyed-mutex texture, composited each frame with a fullscreen
// triangle. That was written around a belief that D2D could not bind the game's
// back buffer (wrong format, missing BGRA flag). Flarial demonstrates it can,
// and the machinery it justified was the source of every hang this client has
// had: two deadlocks on the keyed mutex, plus a per-frame pass that saved and
// restored ten pieces of the game's pipeline state on its own context.
//
// What remains has no second device, no shared surface, no cross-device
// synchronisation, and no state to restore. The only rule it adds is that
// nothing may hold a back-buffer reference when the game calls ResizeBuffers —
// see resize().
namespace glacier::ui {

// 0xAARRGGBB, matching how colors are usually written by hand.
struct Color {
    float r = 0, g = 0, b = 0, a = 1;

    static constexpr Color rgba(std::uint32_t argb) {
        return Color{
            static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
            static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
            static_cast<float>(argb & 0xFF) / 255.0f,
            static_cast<float>((argb >> 24) & 0xFF) / 255.0f,
        };
    }
    constexpr Color withAlpha(float alpha) const { return Color{ r, g, b, alpha }; }
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;

    constexpr float right()  const { return x + w; }
    constexpr float bottom() const { return y + h; }
    constexpr bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    constexpr bool intersects(const Rect& o) const {
        return x < o.x + o.w && x + w > o.x && y < o.y + o.h && y + h > o.y;
    }
    constexpr Rect inset(float d) const { return Rect{ x + d, y + d, w - 2 * d, h - 2 * d }; }
};

enum class TextAlign { Left, Center, Right };

// Which typeface a piece of text is drawn in.
//
// `Icon` is Font Awesome 6 Free (Solid), embedded in the DLL as a resource and
// loaded into a private DirectWrite collection — it is never installed on the
// machine and never visible to anything else in the process. Windows' own
// symbol fonts were the obvious alternative and are the wrong choice here:
// Windows 10 ships Segoe MDL2 Assets, Windows 11 ships Segoe Fluent Icons,
// they disagree on both name and coverage, and this client is tested across
// both. Even so, nothing draws an icon blind — see Renderer::hasGlyph.
enum class FontFamily { UI, Icon };

// Text weight, replacing an earlier `bool bold`.
//
// The bool only offered Normal and SemiBold, and every HUD widget passed true —
// which is why the HUD read as heavy and uniform. Small UI text wants Light or
// Normal; SemiBold is for emphasis against it, not a default. Naming the weights
// also makes a call site say what it wants instead of asserting "bold: yes".
enum class FontWeight { Light, Normal, Medium, SemiBold };

class Renderer {
public:
    static Renderer& get() {
        static Renderer instance;
        return instance;
    }

    // Brings up DirectWrite. The D2D context itself is created from the game's
    // back buffer on the first frame. Idempotent; the parameter is unused and
    // kept so callers need not care which design is behind it.
    bool initialize(ID3D11Device* gameDevice);
    void shutdown();

    // Called from the ResizeBuffers hook, BEFORE the game's own call. Drops
    // every reference we hold to the back buffer. Never a no-op — see the
    // implementation for why skipping it on an unchanged or zero size was a bug.
    void resize(UINT width, UINT height);

    // Frame bracket. beginFrame returns false if the renderer is unusable, in
    // which case the caller must draw nothing and skip endFrame.
    bool beginFrame(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context);
    void endFrame();

    // ── Draw API (valid only between beginFrame/endFrame) ──
    void fillRect(const Rect& r, const Color& c);
    void fillRoundedRect(const Rect& r, float radius, const Color& c);
    void strokeRoundedRect(const Rect& r, float radius, const Color& c, float thickness = 1.0f);
    // `shadow` draws the string once in translucent black, offset a pixel down
    // and right, before the real draw. HUD text sits over arbitrary game
    // content — sky, snow, a white ceiling — and without it light text is
    // simply illegible against a bright background. The game's own HUD does
    // the same for exactly this reason.
    void drawText(std::string_view text, const Rect& box, const Color& c,
                  float size = 14.0f, TextAlign align = TextAlign::Left,
                  FontWeight weight = FontWeight::Normal, bool shadow = false);
    float measureText(std::string_view text, float size = 14.0f,
                      FontWeight weight = FontWeight::Normal);

    // Height of one line at `size`, from the font's own metrics rather than a
    // guessed multiplier. Every widget used to hardcode `size * 1.4f` (or
    // 1.35f — they disagreed), which is why lines in different widgets did not
    // sit on a common rhythm.
    float lineHeight(float size, FontWeight weight = FontWeight::Normal);

    // ── Icons ──
    //
    // Draws one Font Awesome codepoint centred in `box`. Returns false —
    // having drawn nothing — if the embedded font failed to load or does not
    // cover that codepoint, so the caller can put its own mark there instead
    // of a tofu box.
    bool drawGlyph(wchar_t glyph, const Rect& box, const Color& c, float size);

    // Whether drawGlyph would actually draw `glyph`. Cached per codepoint —
    // the lookup goes through DirectWrite and is far too slow to repeat per
    // frame, per icon.
    bool hasGlyph(wchar_t glyph);

    void fillEllipse(float cx, float cy, float rx, float ry, const Color& c);
    void drawLine(float x1, float y1, float x2, float y2, const Color& c, float thickness = 1.0f);

    void pushClip(const Rect& r);
    void popClip();

    // Captures whatever's currently drawn at `r` (the game frame, plus
    // anything Glacier has drawn so far this frame) and redraws it
    // Gaussian-blurred in place. Call BEFORE drawing a translucent panel
    // fill over the same rect — the blur is what shows through the
    // translucency, not a blur of the panel itself. No-op outside
    // beginFrame/endFrame, same as the other draw calls.
    void blurBackdrop(const Rect& r, float sigma = 16.0f);

    float width()  const { return m_width; }
    float height() const { return m_height; }
    bool  ready()  const { return m_ready; }

private:
    Renderer() = default;

    bool createTarget(IDXGISwapChain* swapChain);
    void releaseTarget();

    // Bridges a D3D12-backed swap chain onto the D2D path below via
    // D3D11On12 — the same technique Latite's DXHooks.cpp/Renderer.cpp use.
    // Returns a DXGI surface wrapping the D3D12 back buffer (or null if the
    // swap chain isn't D3D12 either, or the command queue hasn't been
    // captured yet), so createTarget() can hand it to the exact same
    // D2D1CreateDeviceContext call the D3D11 path uses.
    IDXGISurface* createDx12WrappedSurface(IDXGISwapChain* swapChain);

    // Text formats are immutable and reused every frame — creating one per
    // drawText call would allocate thousands of COM objects per second. Cached
    // as a member (not a function-local static) so shutdown() can release them.
    struct FormatKey {
        int  quarterSize;   // size * 4, so half-point sizes still key distinctly
        FontWeight weight;
        TextAlign align;
        FontFamily family;
        bool operator==(const FormatKey& o) const {
            return quarterSize == o.quarterSize && weight == o.weight
                && align == o.align && family == o.family;
        }
    };
    struct FormatKeyHash {
        std::size_t operator()(const FormatKey& k) const noexcept {
            return static_cast<std::size_t>(k.quarterSize) * 64
                 + static_cast<std::size_t>(k.weight) * 16
                 + static_cast<std::size_t>(k.align) * 4
                 + static_cast<std::size_t>(k.family);
        }
    };
    std::unordered_map<FormatKey, IDWriteTextFormat*, FormatKeyHash> m_formats;

    IDWriteTextFormat* formatFor(float size, FontWeight weight, TextAlign align,
                                 FontFamily family = FontFamily::UI);
    void releaseFormats();

    // Loads the embedded Font Awesome resource into a private DirectWrite
    // collection. Runs once, on first icon use; latches on failure so a
    // machine where it doesn't work doesn't retry (and re-log) per frame.
    void ensureIconFont();

    IDWriteFontCollection* m_iconCollection = nullptr;
    IDWriteFont*           m_iconFont       = nullptr;   // for HasCharacter
    std::wstring           m_iconFamily;
    bool                   m_iconResolved   = false;
    std::unordered_map<wchar_t, bool> m_glyphCoverage;

    // Everything below lives on the GAME's device. m_d2d and m_target both hold
    // a reference to the back buffer, which is why resize() drops them.
    IDWriteFactory*       m_dwrite = nullptr;
    ID2D1DeviceContext*   m_d2d    = nullptr;
    ID2D1Bitmap1*         m_target = nullptr;
    ID2D1SolidColorBrush* m_brush  = nullptr;

    // blurBackdrop's scratch bitmap + effect. Both sized/created lazily and
    // reused frame to frame — recreating a bitmap and rebuilding an effect
    // graph every frame the menu is open would be needless GPU churn for
    // something that's the same size almost every call.
    ID2D1Bitmap* m_blurSource = nullptr;
    ID2D1Effect* m_blurEffect = nullptr;
    D2D1_SIZE_U  m_blurSourceSize{ 0, 0 };

    // D3D12 bridge (see createDx12WrappedSurface). The On12 device/context
    // persist across resizes — only the wrapped back buffer is per-target.
    // m_isDx12 sticks once true: the swap chain's backend doesn't change
    // mid-session, so there's no reason to re-probe every frame.
    bool                  m_isDx12            = false;
    ID3D11On12Device*     m_d3d11on12         = nullptr;
    ID3D11Device*         m_d3d11on12Device   = nullptr;
    ID3D11DeviceContext*  m_d3d11on12Context  = nullptr;
    ID3D11Resource*       m_wrappedBackBuffer = nullptr;

    float m_width  = 0;
    float m_height = 0;
    UINT  m_pxWidth  = 0;
    UINT  m_pxHeight = 0;
    bool  m_ready   = false;
    // Latches after a failed device creation so we don't retry — and re-log —
    // on every single frame.
    bool  m_deviceFailed = false;
    bool  m_drawing = false;
    int   m_clipDepth = 0;
};

} // namespace glacier::ui
