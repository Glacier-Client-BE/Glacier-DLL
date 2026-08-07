#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d3d11.h>
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
    constexpr Rect inset(float d) const { return Rect{ x + d, y + d, w - 2 * d, h - 2 * d }; }
};

enum class TextAlign { Left, Center, Right };

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
    void drawText(std::string_view text, const Rect& box, const Color& c,
                  float size = 14.0f, TextAlign align = TextAlign::Left, bool bold = false);
    float measureText(std::string_view text, float size = 14.0f, bool bold = false);

    void pushClip(const Rect& r);
    void popClip();

    float width()  const { return m_width; }
    float height() const { return m_height; }
    bool  ready()  const { return m_ready; }

private:
    Renderer() = default;

    bool createTarget(IDXGISwapChain* swapChain);
    void releaseTarget();

    // Text formats are immutable and reused every frame — creating one per
    // drawText call would allocate thousands of COM objects per second. Cached
    // as a member (not a function-local static) so shutdown() can release them.
    struct FormatKey {
        int  quarterSize;   // size * 4, so half-point sizes still key distinctly
        bool bold;
        TextAlign align;
        bool operator==(const FormatKey& o) const {
            return quarterSize == o.quarterSize && bold == o.bold && align == o.align;
        }
    };
    struct FormatKeyHash {
        std::size_t operator()(const FormatKey& k) const noexcept {
            return static_cast<std::size_t>(k.quarterSize) * 8
                 + static_cast<std::size_t>(k.bold) * 4
                 + static_cast<std::size_t>(k.align);
        }
    };
    std::unordered_map<FormatKey, IDWriteTextFormat*, FormatKeyHash> m_formats;

    IDWriteTextFormat* formatFor(float size, bool bold, TextAlign align);
    void releaseFormats();

    // Everything below lives on the GAME's device. m_d2d and m_target both hold
    // a reference to the back buffer, which is why resize() drops them.
    IDWriteFactory*       m_dwrite = nullptr;
    ID2D1DeviceContext*   m_d2d    = nullptr;
    ID2D1Bitmap1*         m_target = nullptr;
    ID2D1SolidColorBrush* m_brush  = nullptr;

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
