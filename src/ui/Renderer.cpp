#include "Renderer.h"

#include "../hook/D3DHook.h"
#include "../resource.h"
#include "../util/Logger.h"

#include <algorithm>
#include <d2d1effects.h>
#include <dwrite_3.h>
#include <iterator>
#include <set>
#include <string_view>
#include <vector>

#pragma comment(lib, "dxguid.lib")

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

// Address inside this DLL, used only to ask the OS which module that is —
// GetModuleHandleExW(FROM_ADDRESS) needs something it can look up, and a
// function-local static is the smallest honest answer. See ensureIconFont.
const char kModuleAnchor = 0;

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
    // Before m_dwrite: both were created through it, and the formats hold a
    // reference to the icon collection.
    releaseFormats();
    safeRelease(m_iconFont);
    safeRelease(m_iconCollection);
    // Unregistered before release, and only after everything that reads font
    // data through it is gone — same ownership rule as ensureIconFont.
    if (m_iconLoader && m_iconFactory) {
        static_cast<IDWriteFactory5*>(m_iconFactory)->UnregisterFontFileLoader(
            static_cast<IDWriteInMemoryFontFileLoader*>(m_iconLoader));
    }
    safeRelease(m_iconLoader);
    safeRelease(m_iconFactory);
    m_iconFamily.clear();
    m_glyphCoverage.clear();
    m_iconResolved = false;
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
    // Both created FROM m_d2d, so they're invalid the moment it's released —
    // must go before it, not just whenever convenient. Recreated lazily by
    // blurBackdrop() the next time it's called.
    safeRelease(m_blurEffect);
    safeRelease(m_blurSource);
    m_blurSourceSize = { 0, 0 };
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
            // Once, not per frame. createTarget runs on every frame that has
            // no target, so an unguarded log here emitted this line at the
            // frame rate — which in a real log from a D3D12 machine drowned
            // out the one warning that actually said why (no command queue).
            warnOnce("could not get the back buffer as a DXGI surface (D3D11) or bridge it "
                     "via D3D11On12 (D3D12) — the overlay cannot draw. The preceding warning "
                     "names the reason; further occurrences are not logged");
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

void Renderer::blurBackdrop(const Rect& r, float sigma) {
    if (!m_drawing) return;

    const auto pxClampX = [&](float v) {
        return static_cast<UINT32>(std::clamp(v, 0.0f, static_cast<float>(m_pxWidth)));
    };
    const auto pxClampY = [&](float v) {
        return static_cast<UINT32>(std::clamp(v, 0.0f, static_cast<float>(m_pxHeight)));
    };
    const D2D1_RECT_U src{ pxClampX(r.x), pxClampY(r.y), pxClampX(r.right()), pxClampY(r.bottom()) };
    if (src.right <= src.left || src.bottom <= src.top) return;

    const D2D1_SIZE_U size{ src.right - src.left, src.bottom - src.top };
    if (!m_blurSource || m_blurSourceSize.width != size.width
                       || m_blurSourceSize.height != size.height) {
        safeRelease(m_blurSource);
        const auto bitmapProps = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        if (FAILED(m_d2d->CreateBitmap(size, nullptr, 0, bitmapProps, &m_blurSource))
            || !m_blurSource) {
            warnOnce("blurBackdrop: CreateBitmap failed — menu background stays unblurred");
            return;
        }
        m_blurSourceSize = size;
    }

    // Grabs whatever is already drawn at `r` — the game frame plus anything
    // Glacier itself has drawn so far this frame. Must run before the
    // panel's own fill, or it captures the panel instead of what's behind it.
    if (FAILED(m_blurSource->CopyFromRenderTarget(nullptr, m_d2d, &src))) return;

    if (!m_blurEffect) {
        if (FAILED(m_d2d->CreateEffect(CLSID_D2D1GaussianBlur, &m_blurEffect)) || !m_blurEffect) {
            warnOnce("blurBackdrop: CreateEffect(GaussianBlur) failed — menu background "
                     "stays unblurred");
            return;
        }
    }
    m_blurEffect->SetInput(0, m_blurSource);
    m_blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma);
    m_blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);
    // Clamps the blur to the captured rect instead of feathering past its
    // edges — without this, the blur reads as a soft-edged patch rather than
    // a clean panel-shaped one.
    m_blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);

    // Drawn back at the exact spot it was captured from.
    const auto offset = D2D1::Point2F(r.x, r.y);
    m_d2d->DrawImage(m_blurEffect, &offset);
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

void Renderer::ensureIconFont() {
    if (m_iconResolved) return;
    m_iconResolved = true;
    if (!m_dwrite) return;

    // The DLL's own module handle, taken from the address of this function
    // rather than from Glacier::module(). The renderer has no other reason to
    // know about the client object, and asking the OS is one call.
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&kModuleAnchor),
                            &self) || !self) {
        LOG_WARN("could not locate Glacier's own module — menu icons unavailable");
        return;
    }

    const HRSRC found = FindResourceW(self, MAKEINTRESOURCEW(IDR_FONT_ICONS), RT_RCDATA);
    const HGLOBAL loaded = found ? LoadResource(self, found) : nullptr;
    const void* bytes = loaded ? LockResource(loaded) : nullptr;
    const DWORD size = found ? SizeofResource(self, found) : 0;
    if (!bytes || size == 0) {
        LOG_WARN("embedded icon font resource missing — menu icons unavailable");
        return;
    }

    // IDWriteFactory5 (Windows 10 1703+) is what makes this possible without
    // writing the font to disk or installing it: an in-memory font file
    // loader plus a font set built from it yields a collection only this
    // process can see. Nothing about the machine's installed fonts changes.
    IDWriteFactory5* factory5 = nullptr;
    if (FAILED(m_dwrite->QueryInterface(__uuidof(IDWriteFactory5),
                                        reinterpret_cast<void**>(&factory5))) || !factory5) {
        LOG_WARN("DirectWrite is too old for in-memory fonts — menu icons unavailable");
        return;
    }

    IDWriteInMemoryFontFileLoader* loader = nullptr;
    IDWriteFontFile*               file   = nullptr;
    IDWriteFontSetBuilder1*        builder = nullptr;
    IDWriteFontSet*                set    = nullptr;
    IDWriteFontCollection1*        collection = nullptr;

    // The resource stays mapped for the lifetime of the module, so the loader
    // may reference the bytes directly — no owner object and no copy.
    bool ok = SUCCEEDED(factory5->CreateInMemoryFontFileLoader(&loader)) && loader
           && SUCCEEDED(factory5->RegisterFontFileLoader(loader))
           && SUCCEEDED(loader->CreateInMemoryFontFileReference(factory5, bytes, size,
                                                                nullptr, &file)) && file
           && SUCCEEDED(factory5->CreateFontSetBuilder(&builder)) && builder
           && SUCCEEDED(builder->AddFontFile(file))
           && SUCCEEDED(builder->CreateFontSet(&set)) && set
           && SUCCEEDED(factory5->CreateFontCollectionFromFontSet(set, &collection)) && collection;

    // The family name is read back from the font rather than hardcoded:
    // "Font Awesome 6 Free" and "Font Awesome 6 Free Solid" are both in its
    // name table, and which one DirectWrite groups the collection under is
    // its business, not ours.
    if (ok) {
        IDWriteFontFamily* family = nullptr;
        if (collection->GetFontFamilyCount() > 0
            && SUCCEEDED(collection->GetFontFamily(0, &family)) && family) {
            IDWriteLocalizedStrings* names = nullptr;
            if (SUCCEEDED(family->GetFamilyNames(&names)) && names) {
                UINT32 length = 0;
                if (SUCCEEDED(names->GetStringLength(0, &length)) && length > 0) {
                    m_iconFamily.resize(length + 1);
                    if (SUCCEEDED(names->GetString(0, m_iconFamily.data(), length + 1))) {
                        m_iconFamily.resize(length);
                    } else {
                        m_iconFamily.clear();
                    }
                }
                names->Release();
            }
            family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_STRETCH_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, &m_iconFont);
            family->Release();
        }
        ok = !m_iconFamily.empty() && m_iconFont != nullptr;
    }

    if (ok) {
        // The loader and the factory it is registered against are KEPT, not
        // released here. RegisterFontFileLoader deliberately does not take a
        // reference — DirectWrite documents that the application owns the
        // loader and must keep it alive until UnregisterFontFileLoader, so
        // that a loader implemented by the app cannot form a reference cycle
        // with the factory. Releasing it here destroyed the object that every
        // read of the font's actual bytes goes through, and the very next call
        // (IDWriteFont::HasCharacter, which has to read the cmap) went through
        // a freed vtable. The symptom was a log that ended, with no exception
        // and no detach, immediately after the "icon font loaded" line below.
        m_iconLoader  = loader;
        m_iconFactory = factory5;
        loader   = nullptr;
        factory5 = nullptr;

        m_iconCollection = collection;   // kept; released in shutdown()
        collection = nullptr;
        std::string family;
        family.reserve(m_iconFamily.size());
        for (const wchar_t c : m_iconFamily) {
            family.push_back(c < 0x80 ? static_cast<char>(c) : '?');
        }
        LOG_INFO("icon font loaded from the embedded resource ('{}', {} bytes)", family, size);
    } else {
        LOG_WARN("embedded icon font failed to load — the menu will draw its own fallback marks");
        safeRelease(m_iconFont);
        m_iconFamily.clear();
    }

    // Only ever non-null on the failure path now — see the note above.
    safeRelease(collection);
    safeRelease(set);
    safeRelease(builder);
    safeRelease(file);
    if (loader && factory5) factory5->UnregisterFontFileLoader(loader);
    safeRelease(loader);
    safeRelease(factory5);
}

bool Renderer::hasGlyph(wchar_t glyph) {
    if (!m_ready) return false;
    if (const auto it = m_glyphCoverage.find(glyph); it != m_glyphCoverage.end()) return it->second;

    ensureIconFont();

    bool covered = false;
    if (m_iconFont) {
        BOOL has = FALSE;
        if (SUCCEEDED(m_iconFont->HasCharacter(static_cast<UINT32>(glyph), &has))) {
            covered = has != FALSE;
        }
    }

    m_glyphCoverage.emplace(glyph, covered);
    return covered;
}

bool Renderer::drawGlyph(wchar_t glyph, const Rect& box, const Color& c, float size) {
    if (!m_drawing || !hasGlyph(glyph)) return false;

    IDWriteTextFormat* fmt = formatFor(size, FontWeight::Normal, TextAlign::Center,
                                       FontFamily::Icon);
    if (!fmt) return false;

    m_brush->SetColor(toD2D(c));
    m_d2d->DrawTextW(&glyph, 1, fmt,
                     D2D1::RectF(box.x, box.y, box.right(), box.bottom()), m_brush,
                     D2D1_DRAW_TEXT_OPTIONS_NONE);
    return true;
}

void Renderer::fillEllipse(float cx, float cy, float rx, float ry, const Color& c) {
    if (!m_drawing) return;
    m_brush->SetColor(toD2D(c));
    m_d2d->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), m_brush);
}

void Renderer::drawLine(float x1, float y1, float x2, float y2, const Color& c, float thickness) {
    if (!m_drawing) return;
    m_brush->SetColor(toD2D(c));
    m_d2d->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), m_brush, thickness);
}

IDWriteTextFormat* Renderer::formatFor(float size, FontWeight weight, TextAlign align,
                                       FontFamily family) {
    const FormatKey key{ static_cast<int>(size * 4.0f), weight, align, family };
    if (auto it = m_formats.find(key); it != m_formats.end()) return it->second;

    DWRITE_FONT_WEIGHT dw = DWRITE_FONT_WEIGHT_NORMAL;
    switch (weight) {
        case FontWeight::Light:    dw = DWRITE_FONT_WEIGHT_LIGHT;     break;
        case FontWeight::Medium:   dw = DWRITE_FONT_WEIGHT_MEDIUM;    break;
        case FontWeight::SemiBold: dw = DWRITE_FONT_WEIGHT_SEMI_BOLD; break;
        default:                   dw = DWRITE_FONT_WEIGHT_NORMAL;    break;
    }

    // Icon text always resolves through the private collection built from the
    // embedded resource — never the system font list, which does not contain
    // this family and would silently substitute Segoe UI (i.e. tofu).
    const bool icon = (family == FontFamily::Icon);
    if (icon) {
        ensureIconFont();
        if (!m_iconCollection || m_iconFamily.empty()) return nullptr;
        dw = DWRITE_FONT_WEIGHT_BLACK;   // Font Awesome Solid's own weight
    }

    IDWriteTextFormat* fmt = nullptr;
    if (FAILED(m_dwrite->CreateTextFormat(
            icon ? m_iconFamily.c_str() : L"Segoe UI",
            icon ? m_iconCollection : nullptr, dw,
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
