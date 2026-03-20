#pragma once
#include <imgui.h>

namespace HUDStyle {

static constexpr float BG_ALPHA    = 0.65f;
static constexpr float BG_ROUND    = 10.f;
static constexpr float BORDER_SZ   = 1.2f;
static constexpr float FONT_BIG    = 18.f;
static constexpr float FONT_MID    = 14.f;
static constexpr float FONT_SMALL  = 11.f;
static constexpr float PAD_X       = 12.f;
static constexpr float PAD_Y       = 9.f;

static constexpr ImU32 BG       = IM_COL32(10,  10,  10, (int)(255*BG_ALPHA));
static constexpr ImU32 BORDER   = IM_COL32(114,137,218, 80);
static constexpr ImU32 ACCENT   = IM_COL32(114,137,218,255);
static constexpr ImU32 WHITE    = IM_COL32(255,255,255,255);
static constexpr ImU32 GREY     = IM_COL32(180,185,195,200);
static constexpr ImU32 SHADOW   = IM_COL32(  0,  0,  0,180);
static constexpr ImU32 GREEN    = IM_COL32( 72,199,142,255);
static constexpr ImU32 YELLOW   = IM_COL32(255,200, 50,255);
static constexpr ImU32 RED      = IM_COL32(237, 70, 70,255);
static constexpr ImU32 BAR_BG   = IM_COL32( 30, 30, 30,200);

static constexpr ImGuiWindowFlags WIN_FLAGS =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoSavedSettings;

inline void push(float alpha = -1.f) {
    float a = alpha >= 0.f ? alpha : BG_ALPHA;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.039f,0.039f,0.039f,a));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.447f,0.537f,0.855f,0.32f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   { PAD_X, PAD_Y });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   BG_ROUND);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, BORDER_SZ);
}
inline void pop() { ImGui::PopStyleVar(3); ImGui::PopStyleColor(2); }

inline void text(ImDrawList* dl, float sz, ImVec2 p, ImU32 col,
                 const char* t, bool shad = true) {
    if (shad) dl->AddText(nullptr, sz, { p.x+1.5f, p.y+1.5f }, SHADOW, t);
    dl->AddText(nullptr, sz, p, col, t);
}

inline void bar(ImDrawList* dl, float x, float y, float w, float h,
                float ratio, ImU32 fill, float r = 4.f) {
    ratio = ratio < 0.f ? 0.f : ratio > 1.f ? 1.f : ratio;
    dl->AddRectFilled({x,y},{x+w,    y+h}, BAR_BG, r);
    if (ratio > 0.f)
        dl->AddRectFilled({x,y},{x+w*ratio,y+h}, fill, r);
}

inline void drag(ImVec2& pos) {
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
        auto d = ImGui::GetIO().MouseDelta;
        pos.x += d.x; pos.y += d.y;
    }
}

} // namespace HUDStyle
