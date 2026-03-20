#pragma once
#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <wincodec.h>

// IconManager
// ──────────────────────────────────────────────────────────────────────────────
// Loads PNG textures from <dll_dir>/icons/modules/<name>.png using the Windows
// Imaging Component (WIC) and uploads them as D3D11 shader-resource views.
//
// Falls back to a procedurally generated 32×32 colored tile when a file is
// missing so every module always has something to display.
//
// Usage:
//   IconManager::get().init(device, dllDir);       // once, after D3D init
//   ID3D11ShaderResourceView* srv = IconManager::get().get("armorhud");
//   ImGui::Image((ImTextureID)srv, {32,32});
// ──────────────────────────────────────────────────────────────────────────────

class IconManager {
public:
    static IconManager& get();

    // Call once after D3D11 device is available
    void init(ID3D11Device* device, const std::string& baseDir);
    void shutdown();

    // Returns SRV for the named icon (never null after init)
    ID3D11ShaderResourceView* getIcon(const std::string& name);

    bool isReady() const { return m_device != nullptr; }

private:
    IconManager() = default;
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;

    ID3D11ShaderResourceView* loadPNG(const std::wstring& path);
    ID3D11ShaderResourceView* makeProceduralIcon(const std::string& name);
    ID3D11ShaderResourceView* createSRV(const BYTE* rgba, UINT w, UINT h);

    static ImU32 hashColor(const std::string& name);

    ID3D11Device*   m_device  = nullptr;
    std::string     m_baseDir;
    std::unordered_map<std::string, ID3D11ShaderResourceView*> m_cache;
    ID3D11ShaderResourceView* m_fallback = nullptr;
};
