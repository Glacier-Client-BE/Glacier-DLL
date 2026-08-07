#include "Renderer.h"

#include "../util/Logger.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <iterator>
#include <vector>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace glacier::ui {

namespace {

// Keyed-mutex slots. Our device acquires 0 to write, hands ownership to 1; the
// game's device acquires 1 to read, hands it back to 0. Any other protocol
// deadlocks one side or the other.
constexpr UINT64 kKeyWrite = 0;
constexpr UINT64 kKeyRead  = 1;

template <typename T>
void safeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

// Fullscreen triangle generated from SV_VertexID — no vertex/index buffer and
// no input layout to bind or restore.
constexpr char kShaderSource[] = R"(
Texture2D    overlayTex : register(t0);
SamplerState overlaySmp : register(s0);

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut vs_main(uint id : SV_VertexID) {
    VSOut o;
    o.uv  = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float4 ps_main(VSOut i) : SV_TARGET {
    return overlayTex.Sample(overlaySmp, i.uv);
}
)";

ID3DBlob* compile(const char* entry, const char* target) {
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, nullptr,
                                  nullptr, nullptr, entry, target,
                                  D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(hr)) {
        LOG_ERROR("shader '{}' failed to compile: {}", entry,
                  errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no detail");
        safeRelease(errors);
        return nullptr;
    }
    safeRelease(errors);
    return code;
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

} // namespace

bool Renderer::initialize() {
    if (m_ready) return true;

    // Our own device, with the flags D2D requires. This is the whole reason the
    // overlay is robust: we never depend on how the game created its device.
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
    constexpr D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   levels, static_cast<UINT>(std::size(levels)),
                                   D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
    if (FAILED(hr)) {
        LOG_WARN("overlay hardware device failed ({:#x}); trying WARP", static_cast<unsigned>(hr));
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                               levels, static_cast<UINT>(std::size(levels)),
                               D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
    }
    if (FAILED(hr)) {
        LOG_ERROR("could not create overlay device: {:#x}", static_cast<unsigned>(hr));
        return false;
    }

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                           __uuidof(ID2D1Factory1),
                           reinterpret_cast<void**>(&m_d2dFactory));
    if (FAILED(hr)) {
        LOG_ERROR("D2D1CreateFactory failed: {:#x}", static_cast<unsigned>(hr));
        shutdown();
        return false;
    }

    IDXGIDevice* dxgiDevice = nullptr;
    if (FAILED(m_device->QueryInterface(__uuidof(IDXGIDevice),
                                        reinterpret_cast<void**>(&dxgiDevice)))) {
        LOG_ERROR("overlay device has no IDXGIDevice");
        shutdown();
        return false;
    }
    hr = m_d2dFactory->CreateDevice(dxgiDevice, &m_d2dDevice);
    safeRelease(dxgiDevice);
    if (FAILED(hr)) {
        LOG_ERROR("ID2D1Factory1::CreateDevice failed: {:#x}", static_cast<unsigned>(hr));
        shutdown();
        return false;
    }

    hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2d);
    if (FAILED(hr)) {
        LOG_ERROR("CreateDeviceContext failed: {:#x}", static_cast<unsigned>(hr));
        shutdown();
        return false;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(&m_dwrite));
    if (FAILED(hr)) {
        LOG_ERROR("DWriteCreateFactory failed: {:#x}", static_cast<unsigned>(hr));
        shutdown();
        return false;
    }

    m_d2d->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_brush);

    m_ready = true;
    LOG_INFO("overlay renderer initialized (private device + shared surface)");
    return true;
}

void Renderer::shutdown() {
    releaseGameResources();
    releaseSharedSurface();
    releaseFormats();

    safeRelease(m_brush);
    safeRelease(m_dwrite);
    safeRelease(m_d2d);
    safeRelease(m_d2dDevice);
    safeRelease(m_d2dFactory);
    safeRelease(m_context);
    safeRelease(m_device);

    m_ready = false;
    m_drawing = false;
    m_pxWidth = m_pxHeight = 0;
}

void Renderer::releaseSharedSurface() {
    safeRelease(m_target);
    safeRelease(m_writeMutex);
    safeRelease(m_sharedTexture);
    m_sharedHandle = nullptr;   // owned by the texture; not a separate close
}

void Renderer::releaseGameResources() {
    safeRelease(m_srv);
    safeRelease(m_readMutex);
    safeRelease(m_gameTexture);
    safeRelease(m_raster);
    safeRelease(m_sampler);
    safeRelease(m_blend);
    safeRelease(m_ps);
    safeRelease(m_vs);
    m_gameDevice = nullptr;
}

bool Renderer::createSharedSurface(UINT width, UINT height) {
    releaseGameResources();
    releaseSharedSurface();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width  = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;   // the format D2D actually wants
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_sharedTexture);
    if (FAILED(hr)) {
        LOG_ERROR("shared texture creation failed: {:#x}", static_cast<unsigned>(hr));
        return false;
    }

    if (FAILED(m_sharedTexture->QueryInterface(__uuidof(IDXGIKeyedMutex),
                                               reinterpret_cast<void**>(&m_writeMutex)))) {
        LOG_ERROR("shared texture has no keyed mutex");
        releaseSharedSurface();
        return false;
    }

    IDXGIResource* resource = nullptr;
    if (FAILED(m_sharedTexture->QueryInterface(__uuidof(IDXGIResource),
                                               reinterpret_cast<void**>(&resource)))) {
        releaseSharedSurface();
        return false;
    }
    hr = resource->GetSharedHandle(&m_sharedHandle);
    safeRelease(resource);
    if (FAILED(hr) || !m_sharedHandle) {
        LOG_ERROR("GetSharedHandle failed: {:#x}", static_cast<unsigned>(hr));
        releaseSharedSurface();
        return false;
    }

    IDXGISurface* surface = nullptr;
    if (FAILED(m_sharedTexture->QueryInterface(__uuidof(IDXGISurface),
                                               reinterpret_cast<void**>(&surface)))) {
        releaseSharedSurface();
        return false;
    }

    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    hr = m_d2d->CreateBitmapFromDxgiSurface(surface, &props, &m_target);
    safeRelease(surface);
    if (FAILED(hr)) {
        LOG_ERROR("CreateBitmapFromDxgiSurface failed: {:#x}", static_cast<unsigned>(hr));
        releaseSharedSurface();
        return false;
    }

    m_pxWidth  = width;
    m_pxHeight = height;
    m_width  = static_cast<float>(width);
    m_height = static_cast<float>(height);
    return true;
}

bool Renderer::createGameResources(ID3D11Device* device) {
    releaseGameResources();
    m_gameDevice = device;

    if (FAILED(device->OpenSharedResource(m_sharedHandle, __uuidof(ID3D11Texture2D),
                                          reinterpret_cast<void**>(&m_gameTexture)))) {
        LOG_ERROR("game device could not open the shared overlay texture");
        return false;
    }
    if (FAILED(m_gameTexture->QueryInterface(__uuidof(IDXGIKeyedMutex),
                                             reinterpret_cast<void**>(&m_readMutex)))) {
        LOG_ERROR("opened shared texture has no keyed mutex");
        return false;
    }
    if (FAILED(device->CreateShaderResourceView(m_gameTexture, nullptr, &m_srv))) {
        LOG_ERROR("overlay SRV creation failed");
        return false;
    }

    ID3DBlob* vsCode = compile("vs_main", "vs_4_0");
    ID3DBlob* psCode = compile("ps_main", "ps_4_0");
    if (!vsCode || !psCode) {
        safeRelease(vsCode);
        safeRelease(psCode);
        return false;
    }
    const HRESULT hrVs = device->CreateVertexShader(vsCode->GetBufferPointer(),
                                                    vsCode->GetBufferSize(), nullptr, &m_vs);
    const HRESULT hrPs = device->CreatePixelShader(psCode->GetBufferPointer(),
                                                   psCode->GetBufferSize(), nullptr, &m_ps);
    safeRelease(vsCode);
    safeRelease(psCode);
    if (FAILED(hrVs) || FAILED(hrPs)) {
        LOG_ERROR("overlay shader creation failed");
        return false;
    }

    // Premultiplied alpha: D2D writes premultiplied, so source is already
    // scaled by its own alpha and must not be scaled again.
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend  = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp   = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&bd, &m_blend);

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sd, &m_sampler);

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = FALSE;
    device->CreateRasterizerState(&rd, &m_raster);

    return m_blend && m_sampler && m_raster;
}

void Renderer::resize(UINT width, UINT height) {
    if (!m_ready || width == 0 || height == 0) return;
    if (width == m_pxWidth && height == m_pxHeight) return;
    createSharedSurface(width, height);
    // Game-side views referenced the old texture; they are rebuilt lazily on the
    // next beginFrame, which is also where we know the game device again.
    releaseGameResources();
}

bool Renderer::beginFrame(IDXGISwapChain* swapChain, ID3D11Device* device,
                          ID3D11DeviceContext* context) {
    if (!m_ready || !swapChain || !device || !context) return false;

    // Track the back-buffer size from the swap chain itself rather than trusting
    // a cached WM_SIZE: borderless-fullscreen transitions change it without one.
    DXGI_SWAP_CHAIN_DESC scd{};
    if (FAILED(swapChain->GetDesc(&scd))) return false;
    if (scd.BufferDesc.Width == 0 || scd.BufferDesc.Height == 0) return false;

    if (scd.BufferDesc.Width != m_pxWidth || scd.BufferDesc.Height != m_pxHeight) {
        if (!createSharedSurface(scd.BufferDesc.Width, scd.BufferDesc.Height)) {
            m_ready = false;
            return false;
        }
    }
    if (!m_srv || m_gameDevice != device) {
        if (!createGameResources(device)) {
            releaseGameResources();
            return false;
        }
    }

    if (FAILED(m_writeMutex->AcquireSync(kKeyWrite, 1000))) return false;

    m_d2d->SetTarget(m_target);
    m_d2d->BeginDraw();
    m_d2d->Clear(D2D1::ColorF(0, 0.0f));   // fully transparent

    m_drawing = true;
    m_clipDepth = 0;
    m_frameContext = context;
    return true;
}

void Renderer::endFrame() {
    if (!m_drawing) return;

    while (m_clipDepth > 0) popClip();

    const HRESULT hr = m_d2d->EndDraw();
    m_d2d->SetTarget(nullptr);
    m_drawing = false;

    m_writeMutex->ReleaseSync(kKeyRead);

    if (FAILED(hr)) {
        LOG_ERROR("D2D EndDraw failed: {:#x} — dropping overlay surface",
                  static_cast<unsigned>(hr));
        releaseGameResources();
        releaseSharedSurface();
        return;
    }

    composite(m_frameContext);
    m_frameContext = nullptr;
}

void Renderer::composite(ID3D11DeviceContext* context) {
    if (!context || !m_srv || !m_readMutex) return;
    if (FAILED(m_readMutex->AcquireSync(kKeyRead, 1000))) return;

    // Save the game's pipeline state. Restoring it is not optional: leaving our
    // shaders, blend state, or viewport bound corrupts the game's own rendering
    // on the very next draw call.
    ID3D11RenderTargetView* oldRtv = nullptr;
    ID3D11DepthStencilView* oldDsv = nullptr;
    context->OMGetRenderTargets(1, &oldRtv, &oldDsv);

    ID3D11BlendState* oldBlend = nullptr;
    FLOAT oldBlendFactor[4] = {};
    UINT  oldSampleMask = 0;
    context->OMGetBlendState(&oldBlend, oldBlendFactor, &oldSampleMask);

    ID3D11RasterizerState* oldRaster = nullptr;
    context->RSGetState(&oldRaster);

    UINT numViewports = 1;
    D3D11_VIEWPORT oldViewport{};
    context->RSGetViewports(&numViewports, &oldViewport);

    ID3D11VertexShader* oldVs = nullptr;
    ID3D11PixelShader*  oldPs = nullptr;
    context->VSGetShader(&oldVs, nullptr, nullptr);
    context->PSGetShader(&oldPs, nullptr, nullptr);

    ID3D11ShaderResourceView* oldSrv = nullptr;
    context->PSGetShaderResources(0, 1, &oldSrv);
    ID3D11SamplerState* oldSampler = nullptr;
    context->PSGetSamplers(0, 1, &oldSampler);

    ID3D11InputLayout* oldLayout = nullptr;
    context->IAGetInputLayout(&oldLayout);
    D3D11_PRIMITIVE_TOPOLOGY oldTopology{};
    context->IAGetPrimitiveTopology(&oldTopology);

    // Draw the overlay. Render target stays whatever the game had bound — that
    // is the back buffer at Present time, which is exactly where we want it.
    D3D11_VIEWPORT vp{};
    vp.Width = m_width;
    vp.Height = m_height;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
    context->RSSetState(m_raster);

    constexpr FLOAT blendFactor[4] = { 0, 0, 0, 0 };
    context->OMSetBlendState(m_blend, blendFactor, 0xFFFFFFFF);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vs, nullptr, 0);
    context->PSSetShader(m_ps, nullptr, 0);
    context->PSSetShaderResources(0, 1, &m_srv);
    context->PSSetSamplers(0, 1, &m_sampler);
    context->Draw(3, 0);

    // Restore, in reverse.
    ID3D11ShaderResourceView* nullSrv = nullptr;
    context->PSSetShaderResources(0, 1, &nullSrv);

    context->IASetPrimitiveTopology(oldTopology);
    context->IASetInputLayout(oldLayout);
    context->PSSetSamplers(0, 1, &oldSampler);
    context->PSSetShaderResources(0, 1, &oldSrv);
    context->PSSetShader(oldPs, nullptr, 0);
    context->VSSetShader(oldVs, nullptr, 0);
    if (numViewports > 0) context->RSSetViewports(1, &oldViewport);
    context->RSSetState(oldRaster);
    context->OMSetBlendState(oldBlend, oldBlendFactor, oldSampleMask);
    context->OMSetRenderTargets(1, &oldRtv, oldDsv);

    safeRelease(oldLayout);
    safeRelease(oldSampler);
    safeRelease(oldSrv);
    safeRelease(oldPs);
    safeRelease(oldVs);
    safeRelease(oldRaster);
    safeRelease(oldBlend);
    safeRelease(oldDsv);
    safeRelease(oldRtv);

    m_readMutex->ReleaseSync(kKeyWrite);
}

// ─── Draw API ────────────────────────────────────────────────────────────────

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

IDWriteTextFormat* Renderer::formatFor(float size, bool bold, TextAlign align) {
    const FormatKey key{ static_cast<int>(size * 4.0f), bold, align };
    if (auto it = m_formats.find(key); it != m_formats.end()) return it->second;

    IDWriteTextFormat* fmt = nullptr;
    if (FAILED(m_dwrite->CreateTextFormat(
            L"Segoe UI", nullptr,
            bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
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
                        float size, TextAlign align, bool bold) {
    if (!m_drawing || text.empty()) return;
    IDWriteTextFormat* fmt = formatFor(size, bold, align);
    if (!fmt) return;

    const std::wstring wide = widen(text);
    m_brush->SetColor(toD2D(c));
    m_d2d->DrawTextW(wide.c_str(), static_cast<UINT32>(wide.size()), fmt,
                     D2D1::RectF(box.x, box.y, box.right(), box.bottom()), m_brush,
                     D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

float Renderer::measureText(std::string_view text, float size, bool bold) {
    if (!m_ready || text.empty()) return 0.0f;
    IDWriteTextFormat* fmt = formatFor(size, bold, TextAlign::Left);
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
