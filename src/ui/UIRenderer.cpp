#include "UIRenderer.h"

#include "JsBridge.h"
#include "../util/Logger.h"

#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Config.h>
#include <AppCore/Platform.h>
#include <d3dcompiler.h>
#include <filesystem>

#pragma comment(lib, "d3dcompiler.lib")

using namespace ultralight;

namespace glacier {

namespace {

// Fullscreen textured quad. We composite the Ultralight bitmap (premultiplied
// BGRA) over the back buffer with straight alpha blending.
constexpr char kShader[] = R"(
struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; };
struct PSIn  { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

Texture2D    tex : register(t0);
SamplerState smp : register(s0);

PSIn VSMain(VSIn input) {
    PSIn o;
    o.pos = float4(input.pos, 0.0, 1.0);
    o.uv  = input.uv;
    return o;
}

float4 PSMain(PSIn input) : SV_TARGET {
    // Ultralight outputs BGRA premultiplied; swizzle to RGBA.
    float4 c = tex.Sample(smp, input.uv);
    return float4(c.b, c.g, c.r, c.a);
}
)";

struct Vertex {
    float x, y;
    float u, v;
};

// Resolve the directory the DLL was loaded from so we can find ./assets.
std::filesystem::path moduleDir() {
    char path[MAX_PATH]{};
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&moduleDir), &self);
    GetModuleFileNameA(self, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

} // namespace

bool UIRenderer::initialize(ID3D11Device* device, ID3D11DeviceContext*, IDXGISwapChain* swapChain) {
    if (m_initialized) return true;

    DXGI_SWAP_CHAIN_DESC desc{};
    swapChain->GetDesc(&desc);
    m_width  = desc.BufferDesc.Width;
    m_height = desc.BufferDesc.Height;

    // ── Ultralight platform setup ──
    const auto base   = moduleDir();
    const auto assets = (base / "assets").string();

    Config config;
    config.cache_path = (base / "cache").string().c_str();
    Platform::instance().set_config(config);
    Platform::instance().set_font_loader(GetPlatformFontLoader());
    Platform::instance().set_file_system(GetPlatformFileSystem(assets.c_str()));
    Platform::instance().set_logger(GetDefaultLogger((base / "ultralight.log").string().c_str()));

    m_renderer = Renderer::Create();
    if (!m_renderer) {
        LOG_ERROR("Ultralight Renderer::Create failed");
        return false;
    }

    ViewConfig vc;
    vc.is_accelerated = false;   // CPU bitmap path — we upload to our own texture
    vc.is_transparent = true;

    m_view = m_renderer->CreateView(m_width, m_height, vc, nullptr);
    m_view->LoadURL("file:///index.html");
    m_view->Focus();

    m_bridge = std::make_unique<JsBridge>(m_view);

    m_device = device;
    createPipeline(device);
    createSizedResources(device);

    m_initialized = true;
    LOG_INFO("UIRenderer initialized ({}x{})", m_width, m_height);
    return true;
}

void UIRenderer::createPipeline(ID3D11Device* device) {
    // Compile shaders.
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* err    = nullptr;

    D3DCompile(kShader, sizeof(kShader), nullptr, nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vsBlob, &err);
    if (err) { LOG_ERROR("VS compile: {}", static_cast<const char*>(err->GetBufferPointer())); err->Release(); }
    D3DCompile(kShader, sizeof(kShader), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psBlob, &err);
    if (err) { LOG_ERROR("PS compile: {}", static_cast<const char*>(err->GetBufferPointer())); err->Release(); }

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);

    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout);
    vsBlob->Release();
    psBlob->Release();

    // Fullscreen quad in NDC.
    const Vertex verts[6] = {
        { -1.f,  1.f, 0.f, 0.f }, {  1.f,  1.f, 1.f, 0.f }, { -1.f, -1.f, 0.f, 1.f },
        { -1.f, -1.f, 0.f, 1.f }, {  1.f,  1.f, 1.f, 0.f }, {  1.f, -1.f, 1.f, 1.f },
    };
    D3D11_BUFFER_DESC bd{};
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof(verts);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA srd{ verts };
    device->CreateBuffer(&bd, &srd, &m_vertexBuffer);

    D3D11_SAMPLER_DESC sd{};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&sd, &m_sampler);

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable           = TRUE;
    blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blend, &m_blend);
}

void UIRenderer::createSizedResources(ID3D11Device* device) {
    // Dynamic texture matching the view size — CPU-write each frame from the
    // Ultralight bitmap, sampled by the pixel shader.
    D3D11_TEXTURE2D_DESC td{};
    td.Width            = m_width;
    td.Height           = m_height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DYNAMIC;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;
    device->CreateTexture2D(&td, nullptr, &m_texture);
    device->CreateShaderResourceView(m_texture, nullptr, &m_srv);
}

void UIRenderer::releaseSizedResources() {
    auto safeRelease = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    safeRelease(m_rtv);     // depends on the back-buffer size
    safeRelease(m_srv);
    safeRelease(m_texture);
}

// Lazily (re)creates the render-target view over the swap chain's current back
// buffer. m_rtv is dropped on resize so it's rebuilt here against the new
// buffer on the first frame after ResizeBuffers completes.
void UIRenderer::ensureRenderTarget(IDXGISwapChain* swapChain, ID3D11Device* device) {
    if (m_rtv) return;
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)))
        && backBuffer) {
        device->CreateRenderTargetView(backBuffer, nullptr, &m_rtv);
        backBuffer->Release();
    }
}

void UIRenderer::releaseDeviceObjects() {
    auto safeRelease = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    releaseSizedResources();
    safeRelease(m_blend);
    safeRelease(m_sampler);
    safeRelease(m_vertexBuffer);
    safeRelease(m_layout);
    safeRelease(m_ps);
    safeRelease(m_vs);
    m_device = nullptr;
}

void UIRenderer::render(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* ctx) {
    if (!m_initialized) {
        initialize(device, ctx, swapChain);
        return;
    }

    // Drive Ultralight every frame (animations, layout, JS timers).
    m_renderer->Update();
    m_renderer->Render();

    // Feed live HUD data to the page on a throttle (~15 Hz). This is independent
    // of menu visibility — the HUD (FPS/CPS/armor/inventory) is always on.
    constexpr unsigned long long kHudIntervalMs = 66;
    const unsigned long long nowMs = GetTickCount64();
    if (m_bridge && (nowMs - m_lastHudPush) >= kHudIntervalMs) {
        m_lastHudPush = nowMs;
        m_bridge->pushHud();
    }

    // Composite every frame so the HUD stays visible; the page itself decides
    // what's painted (HUD always, menu only when open).
    ensureRenderTarget(swapChain, device);
    uploadAndDraw(ctx);
}

void UIRenderer::uploadAndDraw(ID3D11DeviceContext* ctx) {
    Surface* surface = m_view->surface();
    auto* bitmapSurface = static_cast<BitmapSurface*>(surface);
    RefPtr<Bitmap> bitmap = bitmapSurface->bitmap();

    // Copy the dirty Ultralight bitmap into our dynamic texture.
    {
        void* pixels = bitmap->LockPixels();
        const uint32_t rowBytes = bitmap->row_bytes();
        const uint32_t height   = bitmap->height();

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(ctx->Map(m_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            auto* dst = static_cast<uint8_t*>(mapped.pData);
            const auto* src = static_cast<const uint8_t*>(pixels);
            for (uint32_t y = 0; y < height; ++y) {
                memcpy(dst + y * mapped.RowPitch, src + y * rowBytes, rowBytes);
            }
            ctx->Unmap(m_texture, 0);
        }
        bitmap->UnlockPixels();
        bitmapSurface->ClearDirtyBounds();
    }

    if (!m_rtv) return;

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    const float blendFactor[4] = { 0, 0, 0, 0 };

    D3D11_VIEWPORT vp{};
    vp.Width    = static_cast<float>(m_width);
    vp.Height   = static_cast<float>(m_height);
    vp.MaxDepth = 1.0f;

    ctx->OMSetRenderTargets(1, &m_rtv, nullptr);
    ctx->RSSetViewports(1, &vp);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(m_layout);
    ctx->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->PSSetShader(m_ps, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &m_srv);
    ctx->PSSetSamplers(0, 1, &m_sampler);
    ctx->OMSetBlendState(m_blend, blendFactor, 0xffffffff);
    ctx->Draw(6, 0);
}

void UIRenderer::onResize(UINT width, UINT height) {
    if (!m_initialized || (width == m_width && height == m_height)) return;
    m_width  = width;
    m_height = height;
    if (m_view) m_view->Resize(width, height);

    // Drop the size-dependent GPU resources. The RTV is rebuilt against the new
    // back buffer on the next frame (after ResizeBuffers runs); the staging
    // texture is recreated immediately at the new size.
    releaseSizedResources();
    if (m_device) createSizedResources(m_device);
}

void UIRenderer::setMenuOpen(bool open) {
    m_menuOpen.store(open);
    if (m_view) {
        if (open) m_view->Focus();
        else      m_view->Unfocus();
        if (m_bridge) {
            m_bridge->pushMenuVisible(open);
            if (open) m_bridge->pushState();
        }
    }
}

bool UIRenderer::onWndProc(HWND, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!m_menuOpen.load() || !m_view) return false;

    switch (msg) {
        case WM_MOUSEMOVE: {
            MouseEvent e;
            e.type   = MouseEvent::kType_MouseMoved;
            e.x      = LOWORD(lParam);
            e.y      = HIWORD(lParam);
            e.button = MouseEvent::kButton_None;
            m_view->FireMouseEvent(e);
            return true;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            MouseEvent e;
            e.type   = (msg == WM_LBUTTONDOWN) ? MouseEvent::kType_MouseDown : MouseEvent::kType_MouseUp;
            e.x      = LOWORD(lParam);
            e.y      = HIWORD(lParam);
            e.button = MouseEvent::kButton_Left;
            m_view->FireMouseEvent(e);
            return true;
        }
        case WM_MOUSEWHEEL: {
            ScrollEvent e;
            e.type     = ScrollEvent::kType_ScrollByPixel;
            e.delta_y  = GET_WHEEL_DELTA_WPARAM(wParam);
            m_view->FireScrollEvent(e);
            return true;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR: {
            KeyEvent e;
            e.type = (msg == WM_CHAR) ? KeyEvent::kType_Char
                   : (msg == WM_KEYDOWN) ? KeyEvent::kType_RawKeyDown
                                         : KeyEvent::kType_KeyUp;
            e.virtual_key_code = static_cast<int>(wParam);
            e.native_key_code  = static_cast<int>(lParam);
            GetKeyIdentifierFromVirtualKeyCode(e.virtual_key_code, e.key_identifier);
            m_view->FireKeyEvent(e);
            return true;
        }
        default:
            return false;
    }
}

void UIRenderer::shutdown() {
    if (!m_initialized) return;
    m_bridge.reset();
    m_view  = nullptr;
    m_renderer = nullptr;
    releaseDeviceObjects();
    m_initialized = false;
    LOG_INFO("UIRenderer shut down");
}

} // namespace glacier
