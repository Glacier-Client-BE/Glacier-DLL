#include "Fonts.h"
#include "../core/Glacier.h"
#include "../core/Logger.h"
#include <imgui.h>
#include <Windows.h>

// Generated header: byte array for FA-Solid 900.
#include "fa_solid_900_embedded.h"

#define ICON_MIN_FA 0xe000
#define ICON_MAX_FA 0xf8ff

namespace Glacier {

Fonts& Fonts::get() {
    static Fonts F;
    return F;
}

static ImFont* loadSegoe(float size) {
    ImGuiIO& io = ImGui::GetIO();

    // Try Segoe UI (Windows native) first.
    char winDir[MAX_PATH]{};
    GetWindowsDirectoryA(winDir, MAX_PATH);
    std::string segoe = std::string(winDir) + "\\Fonts\\segoeui.ttf";

    ImFontConfig cfg{};
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH  = true;

    ImFont* base = io.Fonts->AddFontFromFileTTF(segoe.c_str(), size, &cfg);
    if (!base) {
        Logger::get().warn("Fonts", "Segoe UI not found - falling back to default");
        base = io.Fonts->AddFontDefault();
    }

    // Merge FA-Solid into the same logical font, so glyph macros from
    // IconsFontAwesome5.h render inline with text.
    if (fa_solid_900_ttf_size > 0) {
        static const ImWchar faRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig faCfg{};
        faCfg.MergeMode = true;
        faCfg.PixelSnapH = true;
        faCfg.GlyphMinAdvanceX = size; // monospace icon width
        // ImGui takes ownership iff FontDataOwnedByAtlas (default true). Pass a
        // copy via AddFontFromMemoryTTF; ImGui mem-copies internally only when
        // FontDataOwnedByAtlas is true with ImFontConfig::FontData = nullptr.
        // The simplest correct path: set FontDataOwnedByAtlas=false so ImGui
        // does not free our static array.
        faCfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned char*>(fa_solid_900_ttf),
            static_cast<int>(fa_solid_900_ttf_size),
            size, &faCfg, faRanges);
    }
    return base;
}

void Fonts::load() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    sm_ = loadSegoe(FONT_SIZE_SM);
    md_ = loadSegoe(FONT_SIZE_MD);
    lg_ = loadSegoe(FONT_SIZE_LG);
    xl_ = loadSegoe(FONT_SIZE_XL);
    io.FontDefault = md_;
    io.Fonts->Build();
}

} // namespace Glacier
