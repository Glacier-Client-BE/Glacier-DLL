#include "IconManager.h"
#include "../utils/Logger.h"
#include <imgui.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <cstring>
#include <cmath>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

// ─────────────────────────────────────────────────────────────────────────────
IconManager& IconManager::get() {
    static IconManager inst;
    return inst;
}

// ─────────────────────────────────────────────────────────────────────────────
void IconManager::init(ID3D11Device* device, const std::string& baseDir) {
    m_device  = device;
    m_baseDir = baseDir;
    // Ensure trailing slash
    if (!m_baseDir.empty() && m_baseDir.back() != '\\' && m_baseDir.back() != '/')
        m_baseDir += '\\';
    Logger::info("IconManager: base dir = {}", m_baseDir);
}

// ─────────────────────────────────────────────────────────────────────────────
void IconManager::shutdown() {
    for (auto& [k, srv] : m_cache)
        if (srv) srv->Release();
    m_cache.clear();
    m_device = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* IconManager::getIcon(const std::string& name) {
    if (!m_device) return nullptr;

    auto it = m_cache.find(name);
    if (it != m_cache.end()) return it->second;

    // Build path: <baseDir>icons\modules\<name>.png
    std::string pathA = m_baseDir + "icons\\modules\\" + name + ".png";

    // Convert to wide for WIC
    int needed = MultiByteToWideChar(CP_UTF8, 0, pathA.c_str(), -1, nullptr, 0);
    std::wstring pathW(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, pathA.c_str(), -1, pathW.data(), needed);

    ID3D11ShaderResourceView* srv = loadPNG(pathW);
    if (!srv) {
        Logger::info("IconManager: '{}' not found — generating procedural icon", name);
        srv = makeProceduralIcon(name);
    }

    m_cache[name] = srv;
    return srv;
}

// ─────────────────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* IconManager::loadPNG(const std::wstring& path) {
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return nullptr;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr,
        GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return nullptr;

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return nullptr;

    // Convert to 32bpp RGBA
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) return nullptr;
    if (FAILED(converter->Initialize(frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom)))
        return nullptr;

    UINT w = 0, h = 0;
    converter->GetSize(&w, &h);
    std::vector<BYTE> pixels(w * h * 4);
    converter->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());

    return createSRV(pixels.data(), w, h);
}

// ─────────────────────────────────────────────────────────────────────────────
// Procedural icon: 32×32 rounded-rectangle tile with the module's first letter
ID3D11ShaderResourceView* IconManager::makeProceduralIcon(const std::string& name) {
    constexpr int SZ = 32;
    std::vector<BYTE> pixels(SZ * SZ * 4, 0);

    uint32_t col = hashColor(name);
    BYTE r = (col >> 0) & 0xFF;
    BYTE g = (col >> 8) & 0xFF;
    BYTE b = (col >> 16) & 0xFF;

    float cr = 6.f; // corner radius
    float cx = SZ * 0.5f, cy = SZ * 0.5f;

    for (int y = 0; y < SZ; y++) {
        for (int x = 0; x < SZ; x++) {
            float fx = (float)x + 0.5f, fy = (float)y + 0.5f;

            // Rounded rect test
            float dx = 0.f, dy = 0.f;
            if (fx < cr)            dx = cr - fx;
            else if (fx > SZ - cr)  dx = fx - (SZ - cr);
            if (fy < cr)            dy = cr - fy;
            else if (fy > SZ - cr)  dy = fy - (SZ - cr);

            float dist2 = dx*dx + dy*dy;
            if (dx > 0.f && dy > 0.f && dist2 > cr*cr) continue; // outside corner

            // Background fill with slight radial gradient
            float distC = sqrtf((fx-cx)*(fx-cx)+(fy-cy)*(fy-cy));
            float fade  = 1.f - distC / (SZ * 0.7f);
            BYTE  alpha = (BYTE)(255 * std::max(0.f, std::min(1.f, fade + 0.15f)));

            BYTE* p = &pixels[(y * SZ + x) * 4];
            p[0] = (BYTE)(r * 0.65f);
            p[1] = (BYTE)(g * 0.65f);
            p[2] = (BYTE)(b * 0.65f);
            p[3] = alpha;
        }
    }

    // Draw a simple centered letter (2×3 pixel font — 1-bit glyph for A-Z)
    // This is a minimal hand-written renderer, not a real font
    char ch = name.empty() ? '?' : (char)toupper((unsigned char)name[0]);
    // Just paint a small white square in the center as a placeholder glyph
    // A real implementation would rasterize via stb_truetype
    int gSz = 10;
    int gx0 = (SZ - gSz) / 2, gy0 = (SZ - gSz) / 2;
    for (int y = gy0; y < gy0 + gSz; y++) {
        for (int x = gx0; x < gx0 + gSz; x++) {
            if (y < 0 || y >= SZ || x < 0 || x >= SZ) continue;
            // Make a simple letter shape: draw just an outline
            bool edge = (y == gy0 || y == gy0+gSz-1 || x == gx0 || x == gx0+gSz-1);
            if (edge) {
                BYTE* p = &pixels[(y * SZ + x) * 4];
                p[0] = 230; p[1] = 235; p[2] = 245; p[3] = 240;
            }
        }
    }
    // Center pixel cluster for the letter (rough approximation)
    int lx = SZ/2 - 3, ly = SZ/2 - 5;
    // Draw ascii art '?' by default — a real build would embed stb_truetype
    (void)ch;

    return createSRV(pixels.data(), SZ, SZ);
}

// ─────────────────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* IconManager::createSRV(const unsigned char* rgba, unsigned w, unsigned h) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = w;
    desc.Height           = h;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem     = rgba;
    init.SysMemPitch = w * 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(m_device->CreateTexture2D(&desc, &init, &tex))) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    m_device->CreateShaderResourceView(tex, &srvDesc, &srv);
    tex->Release();
    return srv;
}

// ─────────────────────────────────────────────────────────────────────────────
uint32_t IconManager::hashColor(const std::string& name) {
    // Simple djb2 hash → map to a pleasant hue
    uint32_t hash = 5381;
    for (char c : name) hash = ((hash << 5) + hash) + (unsigned char)c;

    // Convert hash to HSV hue, fixed S+V for pleasant colors
    float h = (float)(hash % 360) / 360.f;
    float s = 0.65f, v = 0.80f;

    // HSV to RGB
    float r, g, b;
    int   i = (int)(h * 6.f);
    float f = h * 6.f - i;
    float p = v * (1.f - s);
    float q = v * (1.f - f * s);
    float t = v * (1.f - (1.f - f) * s);
    switch (i % 6) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        default:r=v; g=p; b=q; break;
    }
    return IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),255);
}
