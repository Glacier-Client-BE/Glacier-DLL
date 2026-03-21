#pragma once
// imgui.h must come first — ImTextureID, ImU32 etc. are used below
#include <imgui.h>
#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  IconManager
//  Loads PNG files from <dllDir>\icons\modules\<name>.png using WIC and
//  uploads them as D3D11 SRVs for use with ImGui::Image().
//  Falls back to a procedurally-generated 32×32 coloured tile when a file
//  is missing so every module always has a non-null texture.
// ─────────────────────────────────────────────────────────────────────────────
class IconManager {
public:
    static IconManager& get();

    // Call once after the D3D11 device is available.
    void init(ID3D11Device* device, const std::string& dllDirectory);
    void shutdown();

    // Returns the SRV for the named icon (never nullptr after init).
    ID3D11ShaderResourceView* getIcon(const std::string& name);

    bool isReady() const { return m_device != nullptr; }

private:
    IconManager() = default;
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;

    ID3D11ShaderResourceView* loadPNG(const std::wstring& path);
    ID3D11ShaderResourceView* makeProceduralIcon(const std::string& name);
    ID3D11ShaderResourceView* createSRV(const unsigned char* rgba, unsigned w, unsigned h);

    // Returns a packed RGBA colour derived from the module name string.
    // Returns uint32_t (= ImU32) to avoid needing ImGui types in the header
    // before imgui.h is guaranteed to be parsed.
    static uint32_t hashColor(const std::string& name);

    ID3D11Device*   m_device  = nullptr;
    std::string     m_baseDir;
    std::unordered_map<std::string, ID3D11ShaderResourceView*> m_cache;
};
