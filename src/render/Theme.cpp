#include "Theme.h"
#include "../core/Glacier.h"
#include <imgui.h>

namespace Glacier {

ImVec4 Theme::toVec4(unsigned argb) {
    return ImVec4(
        ((argb >> 16) & 0xFF) / 255.f,
        ((argb >>  8) & 0xFF) / 255.f,
        ( argb        & 0xFF) / 255.f,
        ((argb >> 24) & 0xFF) / 255.f);
}

void Theme::apply() {
    ImGuiStyle& s = ImGui::GetStyle();
    auto v = [](unsigned c) { return Theme::toVec4(c); };

    // Geometry --------------------------------------------------------------
    s.WindowRounding     = ROUND_MD;
    s.ChildRounding      = ROUND_MD;
    s.FrameRounding      = ROUND_SM;
    s.PopupRounding      = ROUND_MD;
    s.ScrollbarRounding  = ROUND_PILL;
    s.GrabRounding       = ROUND_PILL;
    s.TabRounding        = ROUND_SM;

    s.WindowBorderSize   = 0.0f;
    s.ChildBorderSize    = 0.0f;
    s.PopupBorderSize    = 0.0f;
    s.FrameBorderSize    = 0.0f;
    s.TabBorderSize      = 0.0f;

    s.WindowPadding      = ImVec2(16, 16);
    s.FramePadding       = ImVec2(10, 6);
    s.ItemSpacing        = ImVec2(10, 8);
    s.ItemInnerSpacing   = ImVec2(8, 6);
    s.IndentSpacing      = 18.0f;
    s.ScrollbarSize      = 12.0f;
    s.GrabMinSize        = 14.0f;

    s.WindowTitleAlign   = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign    = ImVec2(0.5f, 0.5f);

    // Colours ---------------------------------------------------------------
    auto& col = s.Colors;
    col[ImGuiCol_Text]                  = v(COL_TEXT);
    col[ImGuiCol_TextDisabled]          = v(COL_TEXT_DARK);
    col[ImGuiCol_WindowBg]              = v(COL_BG_BASE);
    col[ImGuiCol_ChildBg]               = v(COL_BG_PANEL);
    col[ImGuiCol_PopupBg]               = v(COL_BG_DEEP);
    col[ImGuiCol_Border]                = v(0x1A000000);
    col[ImGuiCol_BorderShadow]          = ImVec4(0,0,0,0);

    col[ImGuiCol_FrameBg]               = v(COL_BG_DEEP);
    col[ImGuiCol_FrameBgHovered]        = v(0xFF323640);
    col[ImGuiCol_FrameBgActive]         = v(0xFF3B4150);

    col[ImGuiCol_TitleBg]               = v(COL_BG_DEEPEST);
    col[ImGuiCol_TitleBgActive]         = v(COL_BG_DEEP);
    col[ImGuiCol_TitleBgCollapsed]      = v(COL_BG_DEEPEST);

    col[ImGuiCol_MenuBarBg]             = v(COL_BG_PANEL);

    col[ImGuiCol_ScrollbarBg]           = v(0x33000000);
    col[ImGuiCol_ScrollbarGrab]         = v(0x66FFFFFF);
    col[ImGuiCol_ScrollbarGrabHovered]  = v(0x99FFFFFF);
    col[ImGuiCol_ScrollbarGrabActive]   = v(COL_ACCENT);

    col[ImGuiCol_CheckMark]             = v(COL_ACCENT);
    col[ImGuiCol_SliderGrab]            = v(COL_ACCENT);
    col[ImGuiCol_SliderGrabActive]      = v(COL_ACCENT_HOVER);

    col[ImGuiCol_Button]                = v(0xFF323640);
    col[ImGuiCol_ButtonHovered]         = v(COL_ACCENT);
    col[ImGuiCol_ButtonActive]          = v(COL_ACCENT_DEEP);

    col[ImGuiCol_Header]                = v(0xFF323640);
    col[ImGuiCol_HeaderHovered]         = v(0x807289DA);
    col[ImGuiCol_HeaderActive]          = v(COL_ACCENT);

    col[ImGuiCol_Separator]             = v(0x22FFFFFF);
    col[ImGuiCol_SeparatorHovered]      = v(0x447289DA);
    col[ImGuiCol_SeparatorActive]       = v(COL_ACCENT);

    col[ImGuiCol_ResizeGrip]            = v(0x227289DA);
    col[ImGuiCol_ResizeGripHovered]     = v(0x447289DA);
    col[ImGuiCol_ResizeGripActive]      = v(COL_ACCENT);

    col[ImGuiCol_Tab]                   = v(COL_BG_DEEP);
    col[ImGuiCol_TabHovered]            = v(COL_ACCENT);
    col[ImGuiCol_TabActive]             = v(COL_BG_PANEL);
    col[ImGuiCol_TabUnfocused]          = v(COL_BG_DEEPEST);
    col[ImGuiCol_TabUnfocusedActive]    = v(COL_BG_DEEP);

    col[ImGuiCol_PlotLines]             = v(COL_ACCENT);
    col[ImGuiCol_PlotLinesHovered]      = v(COL_ACCENT_HOVER);
    col[ImGuiCol_PlotHistogram]         = v(COL_ACCENT);
    col[ImGuiCol_PlotHistogramHovered]  = v(COL_ACCENT_HOVER);

    col[ImGuiCol_TextSelectedBg]        = v(0x807289DA);
    col[ImGuiCol_DragDropTarget]        = v(COL_ACCENT_HOVER);
    col[ImGuiCol_NavHighlight]          = v(COL_ACCENT);
    col[ImGuiCol_NavWindowingHighlight] = v(COL_ACCENT_HOVER);
    col[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.f, 0.f, 0.f, 0.5f);
    col[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.f, 0.f, 0.f, 0.55f);
}

} // namespace Glacier
