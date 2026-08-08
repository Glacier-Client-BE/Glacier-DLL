#include "Renderer.h"

#include "../hook/D3DHook.h"
#include "../util/Logger.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string_view>
#include <vector>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace glacier::ui {

namespace {

template <typename T>
void safeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

std::wstring widen(std::string_view s) {
    if (s.empty()) return {};
    const int need = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                         nullptr, 0);
    std::wstring out(static_cast<std::size_t>(need), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), need);
    return out;
}

D2D1_COLOR_F toD2D(const Color& c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

// Reports a per-frame failure exactly once. These paths run at frame rate, so
// an unguarded log would emit thousands of lines a second and bury the very
// message it was meant to surface — which is how they ended up silent instead,
// leaving "nothing renders and nothing is logged" as the only symptom.
void warnOnce(const char* message) {
    static std::set<std::string_view> seen;
    if (seen.insert(message).second) {
        LOG_WARN("{}", message);
    }
}

} // namespace

bool Renderer::initialize(ID3D11Device* /*gameDevice*/) {
    if (m_ready) return true;

    // Only DirectWrite is needed up front now. The D2D device context is
    // created from the game's back buffer on the first frame, in createTarget.
    const HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                           __uuidof(IDWriteFactory),
                                           reinterpret_cast<IUnknown**>(&m_dwrite));
    if (FAILED(hr)) {
        LOG_ERROR("DWriteCreateFactory failed: {:#x}", static_cast<unsigned>(hr));
        return false;
    }

    m_ready = true;
    LOG_INFO("overlay renderer initialized (drawing directly onto the game's back buffer)");
    return true;
}

void Renderer::shutdown() {
    releaseTarget();
    releaseFormats();
    safeRelease(m_dwrite);
    safeRelease(m_d3d11on12);
    safeRelease(m_d3d11on12Context);
    safeRelease(m_d3d11on12Device);
    m_isDx12 = false;

    m_ready = false;
    m_drawing = false;
    m_pxWidth = m_pxHeight = 0;
}

void Renderer::releaseTarget() {
    // Order matters: the bitmap and the device context both hold references to
    // the back buffer, and every one of them has to be gone before the game
    // calls ResizeBuffers.
    if (m_d2d) m_d2d->SetTarget(nullptr);
    safeRelease(m_brush);
    safeRelease(m_target);
    safeRelease(m_d2d);
    // Per-target only — m_d3d11on12/m_d3d11on12Device/m_d3d11on12Context are
    // NOT released here. They bridge the game's D3D12 device itself, not any
    // one back buffer, and stay alive across resizes exactly like the game's
    // own D3D12 device does; only the wrapped resource is tied to a specific
    // back buffer and needs recreating.
    safeRelease(m_wrappedBackBuffer);
}

IDXGISurface* Renderer::createDx12WrappedSurface(IDXGISwapChain* swapChain) {
    ID3D12Device* device12 = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device12)))
        || !device12) {
        // Neither a D3D11 surface nor a D3D12 device — nothing more to try.
        return nullptr;
    }

    ID3D12CommandQueue* queue = D3DHook::get().commandQueue();
    if (!queue) {
        device12->Release();
        warnOnce("game is rendering through D3D12 but no command queue has been captured yet "
                 "(see D3DHook's CreateSwapChainForHwnd hook) — overlay unavailable this frame");
        return nullptr;
    }

    if (!m_d3d11on12) {
        constexpr D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        IUnknown* queues[] = { queue };
        // BGRA support is required: D2D draws through a BGRA bitmap, and
        // without this flag the interop device would reject binding one.
        const HRESULT hr = D3D11On12CreateDevice(
            device12, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1,
            reinterpret_cast<IUnknown* const*>(queues), 1, 0,
            &m_d3d11on12Device, &m_d3d11on12Context, nullptr);
        if (FAILED(hr) || !m_d3d11on12Device) {
            device12->Release();
            LOG_ERROR("D3D11On12CreateDevice failed: {:#x} — overlay cannot bridge onto "
                      "this D3D12 swap chain", static_cast<unsigned>(hr));
            return nullptr;
        }
        if (FAILED(m_d3d11on12Device->QueryInterface(__uuidof(ID3D11On12Device),
                                                      reinterpret_cast<void**>(&m_d3d11on12)))
            || !m_d3d11on12) {
            device12->Release();
            LOG_ERROR("ID3D11On12Device QueryInterface failed — overlay cannot bridge onto "
                      "this D3D12 swap chain");
            return nullptr;
        }
        LOG_INFO("bridged the overlay onto the game's D3D12 swap chain via D3D11On12");
    }

    ID3D12Resource* backBuffer12 = nullptr;
    const HRESULT hr = swapChain->GetBuffer(0, __uuidof(ID3D12Resource),
                                            reinterpret_cast<void**>(&backBuffer12));
    device12->Release();
    if (FAILED(hr) || !backBuffer12) {
        LOG_ERROR("could not get the D3D12 back buffer: {:#x}", static_cast<unsigned>(hr));
        return nullptr;
    }

    D3D11_RESOURCE_FLAGS flags{ D3D11_BIND_RENDER_TARGET };
    ID3D11Resource* wrapped = nullptr;
    const HRESULT wrapHr = m_d3d11on12->CreateWrappedResource(
        backBuffer12, &flags, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
        __uuidof(ID3D11Resource), reinterpret_cast<void**>(&wrapped));
    backBuffer12->Release();
    if (FAILED(wrapHr) || !wrapped) {
        LOG_ERROR("CreateWrappedResource failed: {:#x}", static_cast<unsigned>(wrapHr));
        return nullptr;
    }

    IDXGISurface* wrappedSurface = nullptr;
    const HRESULT surfHr = wrapped->QueryInterface(__uuidof(IDXGISurface),
                                                    reinterpret_cast<void**>(&wrappedSurface));
    if (FAILED(surfHr) || !wrappedSurface) {
        LOG_ERROR("wrapped D3D12 back buffer does not expose IDXGISurface: {:#x}",
                  static_cast<unsigned>(surfHr));
        wrapped->Release();
        return nullptr;
    }

    m_wrappedBackBuffer = wrapped;   // released in releaseTarget()
    m_isDx12 = true;
    return wrappedSurface;
}

bool Renderer::createTarget(IDXGISwapChain* swapChain) {
    releaseTarget();

    IDXGISurface* surface = nullptr;
    HRESULT hr = swapChain->GetBuffer(0, __uuidof(IDXGISurface),
                                      reinterpret_cast<void**>(&surface));
    if (FAILED(hr) || !surface) {
        // Not necessarily an error yet — Bedrock's back buffer is only an
        // IDXGISurface when the game is rendering through D3D11. On D3D12 the
        // back buffer is an ID3D12Resource, which does not implement
        // IDXGISurface at all, so this QueryInterface-style failure is
        // completely expected there. Try the D3D12 bridge before giving up.
        surface = createDx12WrappedSurface(swapChain);
        if (!surface) {
            LOG_ERROR("could not get the back buffer as a DXGI surface (D3D11) or bridge it "
                      "via D3D11On12 (D3D12): {:#x}", static_cast<unsigned>(hr));
            return false;
        }
    }

    DXGI_SURFACE_DESC sd{};
    surface->GetDesc(&sd);

    const D2D1_CREATION_PROPERTIES props{
        D2D1_THREADING_MODE_MULTI_THREADED,
        D2D1_DEBUG_LEVEL_NONE,
        D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
    };

    // The one call this whole design rests on: D2D binds the game's back-buffer
    // surface directly, on the game's own device. No second device, no shared
    // texture, no keyed mutex, no compositing pass.
    hr = D2D1CreateDeviceContext(surface, props, &m_d2d);
    if (FAILED(hr) || !m_d2d) {
        LOG_ERROR("D2D1CreateDeviceContext on the back buffer failed: {:#x}",
                  static_cast<unsigned>(hr));
        safeRelease(surface);
        return false;
    }

    // Take the format from the surface rather than assuming one. Flarial pins
    // R8G8B8A8; reading it is the same thing when that is what the game uses,
    // and correct when it is not.
    const auto bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(sd.Format, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    hr = m_d2d->CreateBitmapFromDxgiSurface(surface, bitmapProps, &m_target);
    safeRelease(surface);

    if (FAILED(hr) || !m_target) {
        LOG_ERROR("CreateBitmapFromDxgiSurface failed: {:#x} (back-buffer format {})",
                  static_cast<unsigned>(hr), static_cast<int>(sd.Format));
        releaseTarget();
        return false;
    }

    m_d2d->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_brush);

    m_pxWidth  = sd.Width;
    m_pxHeight = sd.Height;
    m_width    = static_cast<float>(sd.Width);
    m_height   = static_cast<float>(sd.Height);

    LOG_INFO("overlay bound directly to the game's back buffer ({}x{}, format {})",
             sd.Width, sd.Height, static_cast<int>(sd.Format));
    return true;
}

void Renderer::resize(UINT /*width*/, UINT /*height*/) {
    // Unconditional, and before the game's ResizeBuffers runs. ResizeBuffers
    // fails outright if any reference to a back buffer is outstanding, and both
    // the D2D bitmap and the D2D device context hold one.
    //
    // No size comparison and no zero check guard this. Width/height of 0 is
    // DXGI's "keep the current size" and the most common form of the call, and
    // an unchanged size is still a real resize for buffer-count and
    // fullscreen-state changes. Skipping either was a bug.
    releaseTarget();
    m_pxWidth = m_pxHeight = 0;
}

bool Renderer::beginFrame(IDXGISwapChain* swapChain, ID3D11Device* /*device*/,
                          ID3D11DeviceContext* /*context*/) {
    if (!swapChain) return false;

    if (!m_ready && !m_deviceFailed) {
        if (!initialize(nullptr)) {
            m_deviceFailed = true;   // don't retry — and re-log — every frame
            return false;
        }
    }
    if (!m_ready) return false;

    // Sized from the swap chain itself rather than a cached WM_SIZE:
    // borderless-fullscreen transitions change it without one.
    DXGI_SWAP_CHAIN_DESC scd{};
    if (FAILED(swapChain->GetDesc(&scd))) {
        warnOnce("swapchain GetDesc failed — cannot size the overlay");
        return false;
    }
    if (scd.BufferDesc.Width == 0 || scd.BufferDesc.Height == 0) {
        warnOnce("swapchain reports a zero-sized back buffer");
        return false;
    }

    if (!m_target || scd.BufferDesc.Width != m_pxWidth
                  || scd.BufferDesc.Height != m_pxHeight) {
        if (!createTarget(swapChain)) return false;
    }

    // The D3D12 bridge: the wrapped resource has to be "acquired" (handed
    // from the game's D3D12 command queue to the interop D3D11 context)
    // before D2D can draw into it, and "released" back before Present submits
    // the D3D12 command list that actually presents it — see endFrame.
    if (m_isDx12) {
        m_d3d11on12->AcquireWrappedResources(&m_wrappedBackBuffer, 1);
    }

    m_d2d->SetTarget(m_target);
    m_d2d->BeginDraw();

    // Deliberately NOT cleared. The target is the game's own frame now, not a
    // private transparent surface — clearing it would erase the game.

    m_drawing = true;
    m_clipDepth = 0;
    return true;
}

void Renderer::endFrame() {
    if (!m_drawing) return;

    while (m_clipDepth > 0) popClip();

    const HRESULT hr = m_d2d->EndDraw();
    m_d2d->SetTarget(nullptr);
    m_drawing = false;

    if (m_isDx12) {
        // Hand the wrapped resource back to D3D12 and flush the D3D11On12
        // context so the draw commands are actually submitted onto the
        // game's queue before its own Present call runs — without the flush
        // here, D3D11On12 is free to batch them arbitrarily late.
        m_d3d11on12->ReleaseWrappedResources(&m_wrappedBackBuffer, 1);
        m_d3d11on12Context->Flush();
    }

    if (FAILED(hr)) {
        // D2DERR_RECREATE_TARGET is routine — it means the surface went away
        // (device reset, mode change) and the target must be rebuilt. Dropping
        // it is enough; the next beginFrame recreates it.
        if (hr != D2DERR_RECREATE_TARGET) {
            LOG_ERROR("D2D EndDraw failed: {:#x} — rebuilding the target",
                      static_cast<unsigned>(hr));
        }
        releaseTarget();
        m_pxWidth = m_pxHeight = 0;
    }
}

void Renderer::fillRect(const Rect& r, const Color& c) {
    if (!m_drawing) return;
    m_brush->SetColor(toD2D(c));
    m_d2d->FillRectangle(D2D1::RectF(r.x, r.y, r.right(), r.bottom()), m_brush);
}

void Renderer::fillRoundedRect(const Rect& r, float radius, const Color& c) {
    if (!m_drawing) return;
    m_brush->SetColor(toD2D(c));
    m_d2d->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(r.x, r.y, r.right(), r.bottom()), radius, radius), m_brush);
}

void Renderer::strokeRoundedRect(const Rect& r, float radius, const Color& c, float thickness) {
    if (!m_drawing) return;
    m_brush->SetColor(toD2D(c));
    m_d2d->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(r.x, r.y, r.right(), r.bottom()), radius, radius),
        m_brush, thickness);
}

void Renderer::releaseFormats() {
    for (auto& [key, fmt] : m_formats) {
        if (fmt) fmt->Release();
    }
    m_formats.clear();
}

IDWriteTextFormat* Renderer::formatFor(float size, FontWeight weight, TextAlign align) {
    const FormatKey key{ static_cast<int>(size * 4.0f), weight, align };
    if (auto it = m_formats.find(key); it != m_formats.end()) return it->second;

    DWRITE_FONT_WEIGHT dw = DWRITE_FONT_WEIGHT_NORMAL;
    switch (weight) {
        case FontWeight::Light:    dw = DWRITE_FONT_WEIGHT_LIGHT;     break;
        case FontWeight::Medium:   dw = DWRITE_FONT_WEIGHT_MEDIUM;    break;
        case FontWeight::SemiBold: dw = DWRITE_FONT_WEIGHT_SEMI_BOLD; break;
        default:                   dw = DWRITE_FONT_WEIGHT_NORMAL;    break;
    }

    IDWriteTextFormat* fmt = nullptr;
    if (FAILED(m_dwrite->CreateTextFormat(
            L"Segoe UI", nullptr, dw,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            size, L"en-us", &fmt))) {
        return nullptr;
    }

    switch (align) {
        case TextAlign::Center: fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); break;
        case TextAlign::Right:  fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); break;
        default:                fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); break;
    }
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    m_formats.emplace(key, fmt);
    return fmt;
}

void Renderer::drawText(std::string_view text, const Rect& box, const Color& c,
                        float size, TextAlign align, FontWeight weight, bool shadow) {
    if (!m_drawing || text.empty()) return;
    IDWriteTextFormat* fmt = formatFor(size, weight, align);
    if (!fmt) return;

    const std::wstring wide = widen(text);

    // Shadow first, so the real glyphs land on top of it. Offset is a flat
    // pixel rather than a fraction of the size: it is there to separate the
    // glyph edge from whatever is behind it, and that need doesn't grow with
    // the font.
    if (shadow) {
        m_brush->SetColor(toD2D(Color{ 0.0f, 0.0f, 0.0f, c.a * 0.7f }));
        m_d2d->DrawTextW(wide.c_str(), static_cast<UINT32>(wide.size()), fmt,
                         D2D1::RectF(box.x + 1.0f, box.y + 1.0f,
                                     box.right() + 1.0f, box.bottom() + 1.0f),
                         m_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    m_brush->SetColor(toD2D(c));
    m_d2d->DrawTextW(wide.c_str(), static_cast<UINT32>(wide.size()), fmt,
                     D2D1::RectF(box.x, box.y, box.right(), box.bottom()), m_brush,
                     D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

float Renderer::measureText(std::string_view text, float size, FontWeight weight) {
    if (!m_ready || text.empty()) return 0.0f;
    IDWriteTextFormat* fmt = formatFor(size, weight, TextAlign::Left);
    if (!fmt) return 0.0f;

    const std::wstring wide = widen(text);
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(m_dwrite->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                                          fmt, 4096.0f, 128.0f, &layout))) {
        return 0.0f;
    }
    DWRITE_TEXT_METRICS metrics{};
    layout->GetMetrics(&metrics);
    safeRelease(layout);
    return metrics.widthIncludingTrailingWhitespace;
}

float Renderer::lineHeight(float size, FontWeight weight) {
    if (!m_ready) return size * 1.35f;   // pre-init callers get the old guess

    IDWriteTextFormat* fmt = formatFor(size, weight, TextAlign::Left);
    if (!fmt) return size * 1.35f;

    // Measure a real glyph rather than asking the format: an empty layout
    // reports zero height, and DirectWrite has no line-metrics call on
    // IDWriteTextFormat itself.
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(m_dwrite->CreateTextLayout(L"Xg", 2, fmt, 4096.0f, 512.0f, &layout))) {
        return size * 1.35f;
    }
    DWRITE_TEXT_METRICS metrics{};
    layout->GetMetrics(&metrics);
    safeRelease(layout);
    return metrics.height > 0.0f ? metrics.height : size * 1.35f;
}

void Renderer::pushClip(const Rect& r) {
    if (!m_drawing) return;
    m_d2d->PushAxisAlignedClip(D2D1::RectF(r.x, r.y, r.right(), r.bottom()),
                               D2D1_ANTIALIAS_MODE_ALIASED);
    ++m_clipDepth;
}

void Renderer::popClip() {
    if (!m_drawing || m_clipDepth == 0) return;
    m_d2d->PopAxisAlignedClip();
    --m_clipDepth;
}

} // namespace glacier::ui
