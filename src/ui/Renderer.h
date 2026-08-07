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
// ── Why compositing instead of drawing straight onto the back buffer ──
//
// The obvious approach is CreateDxgiSurfaceRenderTarget on the game's back
// buffer. It is also fragile: Direct2D will only bind a surface whose format it
// supports (practically, B8G8R8A8_UNORM), and it requires the *game's* D3D11
// device to have been created with D3D11_CREATE_DEVICE_BGRA_SUPPORT. Neither is
// under our control, and Bedrock's back buffer is commonly R8G8B8A8 — so that
// path can simply refuse to initialize on a perfectly healthy machine.
//
// Instead Glacier owns a second D3D11 device (created with the flags D2D needs),
// renders the UI into a shared B8G8R8A8 texture, and composites that texture
// onto the game's back buffer with a fullscreen triangle. This works regardless
// of the game's back-buffer format or device flags. The cost is one texture copy
// per frame, which is irrelevant next to a menu that is only drawn while open.
//
// Access to the shared texture is serialized with a keyed mutex: our device
// writes it, the game's device reads it, and without the mutex that is a race
// across two devices.
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

    // Creates the private device and D2D/DWrite stack on the same adapter as
    // `gameDevice`. Called lazily from beginFrame, because the game's adapter
    // is not knowable until it hands us its device. Idempotent.
    bool initialize(ID3D11Device* gameDevice);
    void shutdown();

    // Called when the game's back buffer changes size. Drops and recreates the
    // shared surface; safe to call with unchanged dimensions (no-op).
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

    bool createSharedSurface(UINT width, UINT height);
    void releaseSharedSurface();
    bool createGameResources(ID3D11Device* device);
    void releaseGameResources();
    void composite(ID3D11DeviceContext* context);

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

    // ── Our private device (writes the overlay) ──
    ID3D11Device*        m_device   = nullptr;
    ID3D11DeviceContext* m_context  = nullptr;
    ID2D1Factory1*       m_d2dFactory = nullptr;
    ID2D1Device*         m_d2dDevice  = nullptr;
    ID2D1DeviceContext*  m_d2d        = nullptr;
    IDWriteFactory*      m_dwrite     = nullptr;
    ID2D1SolidColorBrush* m_brush     = nullptr;

    // ── Shared surface ──
    ID3D11Texture2D* m_sharedTexture = nullptr;   // owned by m_device
    IDXGIKeyedMutex* m_writeMutex    = nullptr;
    ID2D1Bitmap1*    m_target        = nullptr;
    HANDLE           m_sharedHandle  = nullptr;

    // ── Game-side resources (read the overlay, composite it) ──
    ID3D11Device*             m_gameDevice = nullptr;
    ID3D11Texture2D*          m_gameTexture = nullptr;
    IDXGIKeyedMutex*          m_readMutex   = nullptr;
    ID3D11ShaderResourceView* m_srv         = nullptr;
    ID3D11VertexShader*       m_vs          = nullptr;
    ID3D11PixelShader*        m_ps          = nullptr;
    ID3D11BlendState*         m_blend       = nullptr;
    ID3D11SamplerState*       m_sampler     = nullptr;
    ID3D11RasterizerState*    m_raster      = nullptr;

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
    ID3D11DeviceContext* m_frameContext = nullptr;
};

} // namespace glacier::ui
