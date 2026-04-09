#include "Renderer.h"
#include "../utils/Logger.h"
#include "../utils/ClientConfig.h"

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <IconsFontAwesome5.h>
#include <dxgi.h>
#include <fa_solid_900_embedded.h>

Renderer& Renderer::get() {
    static Renderer instance;
    return instance;
}

bool Renderer::init(ID3D11Device* device, IDXGISwapChain* swapChain) {
    m_device = device;
    device->GetImmediateContext(&m_context);

    if (!createRTV(swapChain)) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io  = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = nullptr;

    // ── Load fonts ────────────────────────────────────────────────────────
    // 1. Default proportional font (Proggy Clean built-in)
    io.Fonts->AddFontDefault();

    // 2. Merge Font Awesome 5 Solid into the default font (embedded in binary)
    {
        static const ImWchar faRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig cfg;
        cfg.MergeMode            = true;
        cfg.PixelSnapH           = true;
        cfg.GlyphMinAdvanceX     = 13.f;
        cfg.FontDataOwnedByAtlas = false;  // data is in static storage — don't free

        if (fa_solid_900_ttf_size > 0) {
            io.Fonts->AddFontFromMemoryTTF(
                const_cast<unsigned char*>(fa_solid_900_ttf),
                static_cast<int>(fa_solid_900_ttf_size),
                13.f, &cfg, faRanges);
            Logger::info("Renderer: Font Awesome loaded from embedded data ({} bytes)",
                         fa_solid_900_ttf_size);
        } else {
            Logger::warn("Renderer: FA font not embedded — icons will be missing");
        }
    }

    io.Fonts->Build();

    // ── ImGui style (Discord palette) ─────────────────────────────────────
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 8.f;
    s.ChildRounding      = 6.f;
    s.FrameRounding      = 4.f;
    s.GrabRounding       = 4.f;
    s.PopupRounding      = 4.f;
    s.ScrollbarRounding  = 4.f;
    s.TabRounding        = 4.f;
    s.WindowBorderSize   = 1.f;
    s.FrameBorderSize    = 0.f;
    s.ItemSpacing        = { 8.f, 6.f };
    s.FramePadding       = { 10.f, 5.f };
    s.WindowPadding      = { 12.f, 12.f };
    s.ScrollbarSize      = 10.f;
    s.GrabMinSize        = 10.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = { 0.137f, 0.153f, 0.165f, 0.97f };
    c[ImGuiCol_ChildBg]              = { 0.172f, 0.184f, 0.200f, 1.f   };
    c[ImGuiCol_PopupBg]              = { 0.137f, 0.153f, 0.165f, 0.97f };
    c[ImGuiCol_Border]               = { 0.447f, 0.537f, 0.855f, 0.40f };
    c[ImGuiCol_FrameBg]              = { 0.137f, 0.153f, 0.165f, 1.f   };
    c[ImGuiCol_FrameBgHovered]       = { 0.447f, 0.537f, 0.855f, 0.30f };
    c[ImGuiCol_FrameBgActive]        = { 0.447f, 0.537f, 0.855f, 0.60f };
    c[ImGuiCol_TitleBg]              = { 0.137f, 0.153f, 0.165f, 1.f   };
    c[ImGuiCol_TitleBgActive]        = { 0.172f, 0.184f, 0.200f, 1.f   };
    c[ImGuiCol_ScrollbarBg]          = { 0.137f, 0.153f, 0.165f, 1.f   };
    c[ImGuiCol_ScrollbarGrab]        = { 0.447f, 0.537f, 0.855f, 0.60f };
    c[ImGuiCol_ScrollbarGrabHovered] = { 0.447f, 0.537f, 0.855f, 0.80f };
    c[ImGuiCol_ScrollbarGrabActive]  = { 0.447f, 0.537f, 0.855f, 1.f   };
    c[ImGuiCol_CheckMark]            = { 0.447f, 0.537f, 0.855f, 1.f   };
    c[ImGuiCol_SliderGrab]           = { 0.447f, 0.537f, 0.855f, 0.80f };
    c[ImGuiCol_SliderGrabActive]     = { 0.447f, 0.537f, 0.855f, 1.f   };
    c[ImGuiCol_Button]               = { 0.447f, 0.537f, 0.855f, 0.30f };
    c[ImGuiCol_ButtonHovered]        = { 0.447f, 0.537f, 0.855f, 0.70f };
    c[ImGuiCol_ButtonActive]         = { 0.447f, 0.537f, 0.855f, 1.f   };
    c[ImGuiCol_Header]               = { 0.447f, 0.537f, 0.855f, 0.30f };
    c[ImGuiCol_HeaderHovered]        = { 0.447f, 0.537f, 0.855f, 0.50f };
    c[ImGuiCol_HeaderActive]         = { 0.447f, 0.537f, 0.855f, 0.80f };
    c[ImGuiCol_Tab]                  = { 0.137f, 0.153f, 0.165f, 1.f   };
    c[ImGuiCol_TabHovered]           = { 0.447f, 0.537f, 0.855f, 0.50f };
    c[ImGuiCol_TabActive]            = { 0.447f, 0.537f, 0.855f, 0.80f };
    c[ImGuiCol_TabUnfocusedActive]   = { 0.447f, 0.537f, 0.855f, 0.50f };
    c[ImGuiCol_Separator]            = { 0.447f, 0.537f, 0.855f, 0.25f };
    c[ImGuiCol_Text]                 = { 1.f,    1.f,    1.f,    1.f   };
    c[ImGuiCol_TextDisabled]         = { 0.600f, 0.667f, 0.710f, 1.f   };

    DXGI_SWAP_CHAIN_DESC desc{};
    swapChain->GetDesc(&desc);
    ImGui_ImplWin32_Init(desc.OutputWindow);
    ImGui_ImplDX11_Init(m_device, m_context);

    m_ready = true;
    Logger::info("Renderer initialized");
    return true;
}

bool Renderer::createRTV(IDXGISwapChain* sc) {
    ID3D11Texture2D* bb = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D),
                             reinterpret_cast<void**>(&bb)))) {
        Logger::error("Renderer: GetBuffer failed");
        return false;
    }
    HRESULT hr = m_device->CreateRenderTargetView(bb, nullptr, &m_rtv);
    bb->Release();
    if (FAILED(hr)) {
        Logger::error("Renderer: CreateRenderTargetView failed: 0x{:08X}",
                      static_cast<uint32_t>(hr));
        return false;
    }
    return true;
}

void Renderer::invalidateRTV() {
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
}

void Renderer::beginFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Renderer::endFrame() {
    ImGui::Render();
    if (m_rtv)
        m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

ImDrawList* Renderer::getBackgroundDrawList() const {
    return ImGui::GetBackgroundDrawList();
}

void Renderer::shutdown() {
    if (!m_ready) return;
    m_ready = false;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    invalidateRTV();
    if (m_context) { m_context->Release(); m_context = nullptr; }
    Logger::info("Renderer shut down");
}
